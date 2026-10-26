"""Local SmartSnap wake-word satellite service. No audio is retained on disk."""
import argparse
import asyncio
import hmac
import json
import logging
import os
from pathlib import Path
from uuid import uuid4

import aiohttp
from aiohttp import web
import numpy as np
from openwakeword.model import Model as WakeModel
from speech_to_phrase.const import Settings
from speech_to_phrase.hass_api import Things
from speech_to_phrase.models import DEFAULT_MODEL
from speech_to_phrase.train import train
from speech_to_phrase.transcribe import transcribe

from audio_state import Utterance

LOG = logging.getLogger('smartsnap.voice')
FRAME_BYTES = 2560  # openWakeWord's 80 ms input


class VoiceService:
    def __init__(self, config, root):
        self.config = config
        self.root = root
        self.connections = set()
        self.recognition_lock = asyncio.Lock()
        self.model = DEFAULT_MODEL  # English en_US-rhasspy in pinned 1.4.3
        sentences = root / 'sentences'
        sentences.mkdir(parents=True, exist_ok=True)
        self.settings = Settings(
            models_dir=root / 'models', train_dir=root / 'train', tools_dir=root / 'tools',
            custom_sentences_dirs=[], hass_token='', hass_websocket_uri='',
            retrain_on_connect=False, sentences_dir=sentences)
        self.replies = {}

    async def prepare(self):
        phrases = [p for command in self.config['commands'] for p in command['phrases']]
        if not phrases or len(phrases) > 500:
            raise ValueError('Configure between 1 and 500 explicit command phrases')
        # JSON is valid YAML. Avoid the upstream built-in Home Assistant commands.
        (self.settings.sentences / 'en.yaml').write_text(json.dumps({
            'language': 'en', 'data': [{'sentences': phrases, 'domains': []}]
        }), encoding='utf-8')
        await train(self.model, self.settings, Things())
        for text in ['Command sent.', 'Voice control is ready.', 'I did not understand that command.',
                     'I could not send that command. Check the device connection.',
                     'That command is not available.', 'Please try again in a moment.']:
            self.replies[text] = await self.synthesize(text)
        LOG.info('Ready: Hey Jarvis, English, %d phrases', len(phrases))

    async def synthesize(self, text):
        # No shell; generated audio only. ffmpeg normalizes espeak's WAV to the wire format.
        voice = await asyncio.create_subprocess_exec('espeak-ng', '--stdout', '-v', 'en-us', '-s', '155', text,
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL)
        wav, _ = await voice.communicate()
        converter = await asyncio.create_subprocess_exec('ffmpeg', '-loglevel', 'error', '-i', 'pipe:0',
            '-f', 's16le', '-ar', '16000', '-ac', '1', 'pipe:1', stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL)
        pcm, _ = await converter.communicate(wav)
        if voice.returncode or converter.returncode or not pcm:
            raise RuntimeError('Could not prepare speaker replies')
        return pcm

    async def recognize(self, pcm, satellite):
        if self.recognition_lock.locked():
            return 'Please try again in a moment.'
        async with self.recognition_lock:
            async def chunks():
                for start in range(0, len(pcm), FRAME_BYTES):
                    yield pcm[start:start + FRAME_BYTES]
            text = await transcribe(self.model, self.settings, chunks())
            LOG.info('Recognized on %s: %s', satellite, text or '(no match)')
            if not text:
                return 'I did not understand that command.'
            async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=5)) as session:
                async with session.post(self.config['hubUrl'],
                    headers={'Authorization': 'Bearer ' + self.config['hubToken']},
                    json={'satellite': satellite, 'text': text, 'requestId': str(uuid4())}) as response:
                    result = await response.json()
                    return result.get('reply', 'That command is not available.')

    async def websocket(self, request):
        satellite_id = request.match_info['satellite']
        satellite = next((s for s in self.config['satellites']
                          if s['id'] == satellite_id and s.get('enabled', True)), None)
        if not satellite or len(satellite.get('token', '')) < 32 or not hmac.compare_digest(
                request.headers.get('Authorization', ''), 'Bearer ' + satellite['token']):
            raise web.HTTPUnauthorized()
        if satellite_id in self.connections or len(self.connections) >= 2:
            raise web.HTTPConflict(text='Voice device already connected or prototype capacity reached')
        self.connections.add(satellite_id)
        ws = web.WebSocketResponse(max_msg_size=4096, heartbeat=15)
        task = None
        try:
            # One model state per microphone. Never mix room audio histories.
            wake = await asyncio.to_thread(WakeModel, wakeword_models=['hey_jarvis'], inference_framework='onnx')
            await ws.prepare(request)
            await ws.send_json({'state': 'idle'})
            utterance = None
            busy = False
            last_playback = 0.0
            noise = 100.0
            loop = asyncio.get_running_loop()

            async def answer(captured):
                nonlocal last_playback
                try:
                    reply = await asyncio.wait_for(self.recognize(bytes(captured.audio), satellite_id), 30) if captured.valid else 'I did not understand that command.'
                except Exception:
                    LOG.exception('Voice processing failed')
                    reply = 'That command is not available.'
                if ws.closed:
                    return
                await ws.send_json({'state': 'speaking', 'reply': reply})
                pcm = self.replies.get(reply, b'')
                for start in range(0, len(pcm), 1280):
                    await ws.send_bytes(pcm[start:start + 1280])
                    await asyncio.sleep(0.04)
                last_playback = loop.time()
                await ws.send_json({'state': 'finished'})

            async for message in ws:
                if message.type == aiohttp.WSMsgType.TEXT:
                    if message.data == 'played' and busy and task and task.done():
                        busy = False
                        wake.reset()
                        await ws.send_json({'state': 'idle'})
                    continue
                if message.type != aiohttp.WSMsgType.BINARY:
                    continue
                if len(message.data) != FRAME_BYTES:
                    await ws.close(code=1003, message=b'Expected 80 ms PCM frames')
                    break
                if busy or loop.time() - last_playback < 0.5:
                    continue
                samples = np.frombuffer(message.data, dtype='<i2')
                rms = float(np.sqrt(np.mean(samples.astype(np.float32) ** 2)))
                if utterance is None:
                    prediction = await asyncio.to_thread(wake.predict, samples)
                    noise = 0.98 * noise + 0.02 * min(rms, 1000)
                    if max(prediction.values(), default=0) >= self.config.get('wakeThreshold', 0.6):
                        utterance = Utterance()
                        await ws.send_json({'state': 'listening'})
                        LOG.info('Wake word: %s', satellite_id)
                else:
                    if utterance.feed(message.data, rms > max(180, noise * 2.5)):
                        busy = True
                        await ws.send_json({'state': 'processing'})
                        task = asyncio.create_task(answer(utterance))
                        utterance = None
        finally:
            self.connections.discard(satellite_id)
            if task:
                task.cancel()
                await asyncio.gather(task, return_exceptions=True)
        return ws


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', required=True)
    parser.add_argument('--data', required=True)
    args = parser.parse_args()
    config = json.loads(Path(args.config).read_text(encoding='utf-8'))
    service = VoiceService(config, Path(args.data))
    await service.prepare()
    app = web.Application()
    app.router.add_get('/audio/{satellite}', service.websocket)
    app.router.add_get('/health', lambda _: web.json_response({'ready': True, 'satellites': len(service.connections)}))
    runner = web.AppRunner(app, access_log=None)
    await runner.setup()
    await web.TCPSite(runner, '0.0.0.0', config.get('port', 10400)).start()
    try:
        await asyncio.Event().wait()
    finally:
        await runner.cleanup()


if __name__ == '__main__':
    os.environ['ORT_DISABLE_TELEMETRY'] = '1'
    logging.basicConfig(level=logging.INFO)
    asyncio.run(main())
