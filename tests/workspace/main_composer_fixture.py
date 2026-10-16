"""Actual-client Main submit regression. Only fresh fictional loopback worlds.

Runs original CSendView via its document-bound SubmitMainInput command. Button
geometry/HWND/selection are covered separately by the native composer probe.
No external window interrogation, input injection or personal profile access.
"""
import argparse
import hashlib
import json
import sqlite3
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--exe', type=Path, default=ROOT/'WinRel/MUSHclient.exe')
p.add_argument('--tabbed-worlds', action='store_true')
p.add_argument('--isolated-desktop', action='store_true', help='Run the candidate on a private desktop without showing or focusing the user desktop')
a = p.parse_args()
child = subprocess.Popen([sys.executable, str(Path(__file__).with_name('modern_client_fixture.py')), '--exe', str(a.exe), '--main-composer-test', '--duration', '150']+(['--tabbed-worlds'] if a.tabbed_worlds else [])+(['--isolated-desktop'] if a.isolated_desktop else []), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
parts=[]
while True:
    line=child.stdout.readline()
    if not line: raise RuntimeError('Fixture did not launch: '+''.join(parts))
    parts.append(line)
    if line.strip()=='}': break
info=json.loads(''.join(parts));dest=Path(info['fixture']);checks=[]
def wait(predicate, timeout=12):
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        result=predicate()
        if result:return result
        time.sleep(.1)
    raise AssertionError('Timed out; evidence at '+str(dest))
def read(path):return path.read_text(encoding='utf-8') if path.exists() else ''
def base(label):return dest/('fixture-plugin.xml.main-'+label)
def action(label,name,value=''):
    receipt=Path(str(base(label))+'.receipt');before=len(read(receipt))
    control=Path(str(base(label))+'.control');pending=Path(str(control)+'.pending')
    pending.write_text(name+'\n'+value,encoding='utf-8',newline='');pending.replace(control)
    result=wait(lambda:read(receipt)[before:] if 'done:'+name+':' in read(receipt)[before:] else None)
    assert 'done:'+name+':ok' in result,result
    return result
def check(name,value):
    checks.append({'name':name,'passed':bool(value)})
    print(('PASS ' if value else 'FAIL ')+name,flush=True)
    assert value,name
def wire(label):return read(dest/(label.lower()+'.received.txt')).splitlines()
try:
    wait(lambda: 'INSTALL Workspace acceptance Beta' in read(dest/'fixture-plugin.xml.receipt.txt'))
    # Server connection precedes first tested send; allow the ordinary tick.
    action('Alpha','draft','alpha-unsent');action('Beta','draft','beta-unsent')
    action('Beta','activate')
    check('two owning characters retain independent original Main drafts','draft:alpha-unsent' in action('Alpha','snapshot') and 'draft:beta-unsent' in action('Beta','snapshot'))
    action('Alpha','draft','redirect');action('Alpha','submit')
    wait(lambda:'OWNER_Alpha' in wire('Alpha'))
    check('callback activation cannot redirect Alpha submission to Beta','OWNER_Alpha' not in wire('Beta') and 'draft:beta-unsent' in action('Beta','snapshot'))
    action('Alpha','draft','recursive');result=action('Alpha','submit')
    wait(lambda:'RECURSIVE_ONCE' in wire('Alpha'))
    check('recursive Main submit is rejected before queuing another send','nested-result:0' not in result and 'nested-result:' in result and wire('Alpha').count('RECURSIVE_ONCE')==1)
    before=list(wire('Alpha'));action('Alpha','draft','decline');action('Alpha','submit')
    check('declined command remains editable without transmission','draft:decline' in action('Alpha','snapshot') and wire('Alpha')==before)
    action('Alpha','draft','clear');action('Alpha','submit')
    check('clear sentinel clears only original draft without transmission','draft:\n' in action('Alpha','snapshot') and wire('Alpha')==before)
    action('Alpha','draft','+alpha alias value');action('Alpha','submit');wait(lambda:'+alpha ALIAS_value' in wire('Alpha'))
    check('original alias pipeline executes once',wire('Alpha').count('+alpha ALIAS_value')==1)
    action('Beta','draft','+beta hook');action('Beta','submit');wait(lambda:'+beta HOOK_TRANSFORMED' in wire('Beta'))
    check('original command-entered transform and command history survive','history:+beta HOOK_TRANSFORMED' in action('Beta','snapshot'))
    for height in (24,96,180,48):action('Alpha','height',str(height))
    action('Alpha','draft','retained-through-height');action('Alpha','height','120')
    check('scripted native body height preserves Main draft','draft:retained-through-height' in action('Alpha','snapshot'))
    action('Alpha','fullscreen')
    check('fullscreen retains owning Main draft','draft:retained-through-height' in action('Alpha','snapshot'))
    action('Alpha','fullscreen')
    action('Alpha','autoheight','1');action('Alpha','draft','line one\r\nline two\r\nline three')
    check('automatic input growth retains exact original multiline draft','draft:line one\nline two\nline three\n' in action('Alpha','snapshot'))
    action('Alpha','autoheight','0');action('Alpha','height','120')
    action('Beta','activate');action('Alpha','activate')
    action('Alpha','fullscreen');action('Alpha','fullscreen')
    action('Alpha','activate');assert 'active:true' in action('Alpha','snapshot')
    # Draft-discard protection has its own cancel/discard fixture. This suite
    # closes an intentionally empty input while checking saved body geometry.
    action('Alpha','draft','');assert 'draft:\n' in action('Alpha','snapshot')
    action('Alpha','close');time.sleep(.4)
    first_close=action('Beta','snapshot')
    assert 'alpha-open:false' in first_close,first_close
    action('Beta','draft','AFTER_OTHER_CLOSE');action('Beta','submit');wait(lambda:'AFTER_OTHER_CLOSE' in wire('Beta'))
    check('closing other character leaves remaining owner functional','AFTER_OTHER_CLOSE' not in wire('Alpha'))
    with sqlite3.connect(dest/'mushclient_prefs.sqlite') as db:
        saved=db.execute('SELECT value FROM worlds WHERE name=?',('Workspace acceptance Alpha:Bottom Height',)).fetchone()
    check('activation fullscreen and normal close retain requested body height without added chrome',saved is not None and int(saved[0])==120)
    action('Beta','reopenalpha');action('Beta','activate');action('Alpha','activate')
    assert 'active:true' in action('Alpha','snapshot')
    action('Alpha','close')
    closed=action('Beta','snapshot') # Allow the ordinary posted Alpha close to finish.
    with sqlite3.connect(dest/'mushclient_prefs.sqlite') as db:
        reopened=db.execute('SELECT value FROM worlds WHERE name=?',('Workspace acceptance Alpha:Bottom Height',)).fetchone()
    check('reopening original character restores body height without cumulative strip growth','alpha-open:false' in closed and reopened is not None and int(reopened[0])==120)
finally:
    (dest/'main-composer-results.json').write_text(json.dumps({'candidate':info,'tabbed_worlds':a.tabbed_worlds,'sha256':hashlib.sha256(a.exe.read_bytes()).hexdigest(),'checks':checks},indent=2),encoding='utf-8')
    (dest/'fixture.control').write_text('stop',encoding='ascii')
    child.wait(timeout=15)
    print('Evidence: '+str(dest),flush=True)
