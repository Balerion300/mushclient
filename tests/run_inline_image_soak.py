"""Build and run an isolated, loopback-only native image-loader soak.

Usage: C:\Python311\python.exe tests\run_inline_image_soak.py --seconds 660
Requires the Visual Studio MFC toolchain and Pillow. Outputs CSV and JSON
measurements; no installed MUSHclient process or files are touched.
"""
import argparse
import csv
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import statistics
import subprocess
import tempfile
import threading

from inline_image_fixture import Handler, ThreadingHTTPServer
from run_native_tests import ROOT, compiler_environment


def summarize(rows, duration, warmup):
    released = [r for r in rows if r['phase'] == 'released']
    post_warmup = [r for r in released if int(r['elapsed_ms']) >= warmup * 1000]
    if not post_warmup:
        raise ValueError('no post-warmup released samples')
    early_start = warmup * 1000
    early = [r for r in post_warmup if int(r['elapsed_ms']) < early_start + 60000]
    late_start = max(early_start, (duration - 60) * 1000)
    late = [r for r in post_warmup if int(r['elapsed_ms']) >= late_start]
    if not early or not late:
        raise ValueError('missing early or late samples')
    memory = lambda items: [int(r['private_bytes']) for r in items]
    gdi = lambda items: [int(r['gdi_handles']) for r in items]
    first_mean = statistics.mean(memory(early))
    last_mean = statistics.mean(memory(late))
    gdi_baseline = min(gdi(early))
    summary = {
        'duration_seconds': duration,
        'warmup_seconds': warmup,
        'cycles': len(released),
        'unique_requests': len(released) * 16,
        'sample_count_post_warmup': len(post_warmup),
        'released_private_bytes_min': min(memory(post_warmup)),
        'released_private_bytes_max': max(memory(post_warmup)),
        'first_minute_mean_private_bytes': round(first_mean),
        'last_minute_mean_private_bytes': round(last_mean),
        'mean_private_bytes_growth': round(last_mean - first_mean),
        'released_gdi_min': min(gdi(post_warmup)),
        'released_gdi_max': max(gdi(post_warmup)),
        'released_gdi_first_minute_min': gdi_baseline,
        'released_gdi_last_minute_max': max(gdi(late)),
        'all_references_expired': all(int(r['expired']) == 16 for r in released),
    }
    # Allow allocator high-water retention while rejecting sustained growth.
    summary['memory_growth_within_32_mib'] = last_mean - first_mean <= 32 * 1024 * 1024
    summary['gdi_handles_within_four'] = max(gdi(late)) <= gdi_baseline + 4
    summary['passed'] = (summary['all_references_expired'] and
                         summary['memory_growth_within_32_mib'] and
                         summary['gdi_handles_within_four'])
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=660)
    parser.add_argument('--warmup', type=int, default=60)
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    if args.seconds < 10 or args.seconds > 3600 or args.warmup < 0 or args.warmup >= args.seconds:
        parser.error('seconds must be 10..3600 and warmup must be less than seconds')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    output = args.output_dir or Path(tempfile.mkdtemp(prefix='mushclient-inline-soak-'))
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    exe = output / 'inline_image_soak.exe'
    csv_path = output / f'inline_image_soak_{stamp}.csv'
    json_path = output / f'inline_image_soak_{stamp}.json'
    env = compiler_environment()
    compiler = Path(env['VCToolsInstallDir']) / 'bin/Hostx86/x86/cl.exe'
    command = [str(compiler), '/nologo', '/std:c++14', '/EHsc', '/MT', '/DWIN32', '/D_WINDOWS',
               '/D_WIN32_WINNT=0x0601', '/DLUA51', '/D_CRT_SECURE_NO_WARNINGS',
               '/DMUSHCLIENT_INLINE_IMAGE_TEST_ALLOW_LOOPBACK', '/I' + str(ROOT),
               str(ROOT / 'tests/inline_image_soak.cpp'), str(ROOT / 'InlineImage.cpp'),
               '/Fe:' + str(exe), '/Fo:' + str(output) + os.sep,
               '/link', '/SUBSYSTEM:CONSOLE']
    subprocess.run(command, cwd=output, env=env, check=True)
    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        result = subprocess.run([str(exe), f'http://127.0.0.1:{server.server_port}',
                                 str(args.seconds), str(csv_path)], timeout=args.seconds + 90)
    finally:
        server.shutdown()
        server.server_close()
    with csv_path.open(newline='') as stream:
        rows = list(csv.DictReader(stream))
    summary = summarize(rows, args.seconds, args.warmup)
    summary['native_exit_code'] = result.returncode
    summary['passed'] = summary['passed'] and result.returncode == 0
    summary['created_utc'] = stamp
    summary['csv_path'] = str(csv_path)
    json_path.write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary, indent=2))
    print('Evidence:', json_path)
    return 0 if summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
