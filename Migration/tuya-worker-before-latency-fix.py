"""One bounded local operation. Credentials enter via stdin, never argv or logs."""
import contextlib
import ipaddress
import json
import re
import sys
import tinytuya
from tinytuya import scanner


def private_ip(value):
    address = ipaddress.ip_address(value)
    if address.version != 4 or not any(address in ipaddress.ip_network(n) for n in
            ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16")):
        raise ValueError("Enter the plug's private LAN IPv4 address")
    return str(address)


METRIC_CODES = {"cur_power": "PowerWatts", "cur_voltage": "Voltage", "cur_current": "Current"}

def metrics(mapping):
    result = {}
    for dp, spec in mapping.items():
        name = METRIC_CODES.get(spec.get("code"))
        values = spec.get("values", {})
        if isinstance(values, str):
            try: values = json.loads(values)
            except ValueError: continue
        if name and str(dp).isdigit() and isinstance(values, dict):
            scale = values.get("scale", 0)
            if isinstance(scale, int) and 0 <= scale <= 6:
                result[name] = {"dp": str(dp), "scale": scale}
    return result

def readings(dps, mapping):
    result = {}
    for name, spec in mapping.items():
        raw = dps.get(str(spec["dp"]))
        if name in METRIC_CODES.values() and type(raw) in (int, float) and raw >= 0:
            result[name] = raw / (10 ** spec["scale"])
    return result

def local(entry, write=None, devices=None):
    address = entry.get("ip", "").strip()
    if not address:
        found = scanner.devices(verbose=False, scantime=12, color=False, poll=False,
                                byID=True, wantids=[entry["id"]], show_timer=False,
                                forcescan=True, assume_yes=True,
                                tuyadevices=devices or [entry])
        candidate = found.get(entry["id"], {})
        address = candidate.get("ip", "")
        if not address:
            raise ValueError("Automatic discovery could not find this plug. Check that it is on the Pi's network, or enter its IP manually.")
    address = private_ip(address)
    versions = [entry["version"]] if entry.get("version") else [3.3, 3.4, 3.5, 3.1]
    for version in versions:
        device = tinytuya.OutletDevice(entry["id"], address, entry["key"], version=version)
        device.set_socketTimeout(2)
        device.set_socketRetryLimit(1)
        try:
            result = device.status()
            value = result.get("dps", {}).get(str(entry["dp"])) if isinstance(result, dict) else None
            if type(value) is not bool:
                continue
            if write is not None:
                device.set_status(write, switch=int(entry["dp"]))
                result = device.status()
                value = result.get("dps", {}).get(str(entry["dp"])) if isinstance(result, dict) else None
                if type(value) is not bool or value != write:
                    raise ValueError("Plug did not confirm the requested state")
            return {"power": value, "version": version, "ip": address, "measurements": readings(result.get("dps", {}), entry.get("metrics", {}))}
        finally:
            device.close()
    raise ValueError("Local connection failed. Check IP, local key, and close Smart Life before retrying.")


def discover(project):
    region = project.get("region", "eu")
    if region not in ("eu", "eu-w", "us", "us-e", "cn", "in", "sg"):
        raise ValueError("Unsupported Tuya region")
    cloud = tinytuya.Cloud(apiRegion=region, apiKey=project["clientId"], apiSecret=project["secret"])
    devices = cloud.getdevices(verbose=True)
    if isinstance(devices, dict) and str(devices.get("code")) == "28841002":
        raise ValueError("Tuya IoT Core subscription has expired (28841002). In the Tuya Developer Platform, open Cloud > My Services > IoT Core and request a trial extension, then retry.")
    if (not isinstance(devices, dict) or devices.get("Error") or devices.get("success") is False
            or not isinstance(devices.get("result"), list)):
        raise ValueError("Tuya device access failed. Check project region, credentials, API subscription and Smart Life account link.")
    supported = []
    for device in devices["result"]:
        if device.get("category") not in ("cz", "pc") or device.get("sub"):
            continue
        mapping = cloud.getmapping(device.get("product_id", ""), device["id"]) or {}
        switches = [(dp, spec) for dp, spec in mapping.items()
                    if spec.get("code") in ("switch", "switch_1") and str(dp).isdigit()]
        other_switches = [spec for spec in mapping.values()
                          if re.fullmatch(r"switch_\d+", spec.get("code", "")) and spec.get("code") != "switch_1"]
        if len(switches) != 1 or other_switches or not device.get("local_key"):
            continue
        supported.append({"id": device["id"], "name": device.get("name", "Smart plug"),
                          "key": device["local_key"], "dp": int(switches[0][0]), "metrics": metrics(mapping)})
    return {"devices": supported}


def main():
    data = json.load(sys.stdin)
    # Suppress third-party diagnostics, which can contain device keys.
    with contextlib.redirect_stdout(sys.stderr):
        if data["action"] == "discover":
            result = discover(data["project"])
        else:
            result = local(data["entry"], data.get("write"), data.get("devices"))
    print(json.dumps({"status": "OK", **result}))


if __name__ == "__main__":
    try:
        main()
    except ValueError as error:
        print(json.dumps({"status": "Error", "message": str(error)}))
    except Exception:
        print(json.dumps({"status": "Error", "message": "Tuya operation failed. Check project permissions, connection and device key."}))
