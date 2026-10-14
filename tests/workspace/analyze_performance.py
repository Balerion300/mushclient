"""Summarize QPC receipts and late idle resource trends; no player data."""
import argparse,csv,json,pathlib,statistics
p=argparse.ArgumentParser();p.add_argument('directory',type=pathlib.Path);a=p.parse_args()
def distribution(values):
    v=sorted(values)
    if not v:return {'count':0}
    def q(f):return v[min(len(v)-1,int((len(v)-1)*f))]
    return {'count':len(v),'p50_ms':q(.5),'p95_ms':q(.95),'p99_ms':q(.99),'max_ms':v[-1]}
rows=list(csv.DictReader((a.directory/'performance.xml.csv').open()))
resources=[json.loads(line) for line in (a.directory/'resources.jsonl').read_text().splitlines()]
sent=json.loads((a.directory/'sent.json').read_text())
config=json.loads((a.directory/'config.json').read_text()) if (a.directory/'config.json').exists() else None
end=[r for r in rows if r['kind']=='output'];received={int(r['sequence']) for r in end}
idle=[]
for cycle in range(10):
    selected=[r for r in resources if r['phase']=='idle' and cycle*120+110<=r['elapsed']<cycle*120+120]
    if selected:idle.append({'cycle':cycle,'elapsed':statistics.mean(r['elapsed'] for r in selected),**{k:statistics.mean(r[k] for r in selected) for k in ('private','working_set','gdi','user','handles')}})
summary={'output_latency':distribution([(float(r['now'])-float(r['sent']))*1000 for r in end]),'script_execute_echo_latency':distribution([(float(r['now'])-float(r['sent']))*1000 for r in rows if r['kind']=='input']),
         'gui_timer_gap':distribution([(float(r['now'])-float(r['sent']))*1000 for r in rows if r['kind']=='tick']),
         'batches_sent':len(sent),'batches_completed':len(received),'last_completed':max(received,default=0),'offered_lines':sum(r['lines'] for r in sent),'last_reported_received_lines':int(end[-1]['total']) if end else 0,
         'idle_cycles':idle,'resource_peak':{k:max(r[k] for r in resources) for k in ('private','working_set','gdi','user','handles')},'elapsed':resources[-1]['elapsed'],
         'cpu_seconds':resources[-1]['cpu']-resources[0]['cpu'],'cpu_one_core_percent':100*(resources[-1]['cpu']-resources[0]['cpu'])/(resources[-1]['time']-resources[0]['time'])}
if len(idle)>2:
    late=idle[1:];summary['late_idle_private_range']=max(r['private'] for r in late)-min(r['private'] for r in late)
    summary['late_idle_slope_bytes_per_minute']=(late[-1]['private']-late[0]['private'])*60/(late[-1]['elapsed']-late[0]['elapsed'])
summary['complete_without_backpressure']=len(sent)==len(received) and not (a.directory/'overload.json').exists()
if config:
    summary['configuration']=config
    completion=a.directory/'completion.json'
    summary['requested_duration_completed']=(json.loads(completion.read_text())['requested_duration_completed'] if completion.exists() else
        max((r['elapsed'] for r in resources if r['phase']!='drain'),default=0)>=config['seconds']-1.5 and not (a.directory/'overload.json').exists() and not (a.directory/'stop').exists())
image_file=a.directory/'performance.xml.images.csv'
if image_file.exists():
    images=list(csv.DictReader(image_file.open()))
    summary['images']={'ready':sum(r['state']=='1' for r in images),'failed':sum(r['state']=='2' for r in images),'pruned_before_receipt':sum(r['state']=='-1' for r in images)}
phases={r['sequence']:r['phase'] for r in sent}
summary['phases']={phase:{kind:distribution([(float(r['now'])-float(r['sent']))*1000 for r in rows if r['kind']==kind and phases.get(int(r['sequence']))==phase]) for kind in ('output','input')} for phase in ('plain','styled','images','idle')}
for phase,details in summary['phases'].items():
    intervals=[(r['cpu']-previous['cpu'],r['time']-previous['time']) for previous,r in zip(resources,resources[1:]) if previous['phase']==phase and r['phase']==phase]
    if intervals:details['cpu_one_core_percent']=100*sum(x for x,_ in intervals)/sum(y for _,y in intervals)
(a.directory/'summary.json').write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
