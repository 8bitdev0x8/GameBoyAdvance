from pathlib import Path
import importlib.util,json,statistics
project=Path(__file__).resolve().parents[1]
(project/'output').mkdir(exist_ok=True)
map_dir=project/'output'
if not list(map_dir.glob('pc-profile-map-v*.json')):map_dir=project.parent/'docs/evidence'
spec=importlib.util.spec_from_file_location('debug_log',project/'tools/debug_log.py')
decoder=importlib.util.module_from_spec(spec);spec.loader.exec_module(decoder)
results=[];unique={};overlapping=0
for path in sorted((project/'logs').glob('sonic-v44*.bin')):
    data=path.read_bytes();header0=decoder.read_header(data[:64])
    map_path=decoder.matching_map(header0['map_fingerprint'],map_dir)
    header,frames,profiles=decoder.decode(data,map_path)
    for frame in frames:
        key=(header['map_fingerprint'],frame['frame'],frame['uptime_ms'])
        if key in unique:
            assert frame==unique[key],('Changed duplicate frame',key)
            overlapping+=1
        else:unique[key]=frame
    summary=decoder.summarize(header,frames,profiles)
    row=dict(file=path.name,bytes=len(data),header=header,frames=len(frames),profiles=len(profiles),
             exact_map=str(map_path) if map_path else None)
    if not frames:
        row['note']='No retained frame records; cannot measure FPS or drawing.'
        results.append(row);print(path.name,row['note']);continue
    wall=sum(f['wall_us'] for f in frames)
    means=summary['mean_ms_per_frame']
    valid=[p for p in profiles if not p['includes_export_pause']]
    sample_count=sum(p['samples'] for p in valid)
    functions={}
    for p in valid:
        for i in range(1,4):
            if p[f'percent{i}']:
                name=p[f'function{i}'];functions[name]=functions.get(name,0)+p['samples']*p[f'percent{i}']/100
    row.update(weighted_fps=summary['weighted_fps'],retained_wall_seconds=wall/1e6,mean_ms=means,
        mix_fraction_of_draw=means['mix']/means['draw'] if means['draw'] else 0,
        run_minus_render_wait_ms=means['run']-means['wait'],
        median_frame_ms=statistics.median(f['wall_ms'] for f in frames),
        p95_frame_ms=sorted(f['wall_ms'] for f in frames)[int(.95*(len(frames)-1))],
        worst_frame_ms=max(f['wall_ms'] for f in frames),
        irq_ms_per_wall_second=sum(f['hdmi_irq_us'] for f in frames)/wall*1000,
        decode_ms_per_wall_second=sum(f['decode_us'] for f in frames)/wall*1000,
        bios_sram_frames=sum(f['bios_sram'] for f in frames),blocks_on_frames=sum(f['blocks_on'] for f in frames),
        pad_on_frames=sum(f['pad_on'] for f in frames),
        ifetch_hit_percent=100*sum(f['ifetch_hits'] for f in frames)/max(1,sum(f['ifetch_hits']+f['ifetch_fills'] for f in frames)),
        mean_dirty_scanlines=statistics.mean(f['dirty_jobs'] for f in frames),
        first_frame=frames[0]['frame'],last_frame=frames[-1]['frame'],
        first_uptime_ms=frames[0]['uptime_ms'],last_uptime_ms=frames[-1]['uptime_ms'],
        first_pc=hex(frames[0]['guest_pc']),last_pc=hex(frames[-1]['guest_pc']),
        mean_ifetch_fills=statistics.mean(f['ifetch_fills'] for f in frames),
        total_rom_misses=sum(f['rom_misses'] for f in frames),
        hdmi_resets_min=min(f['hdmi_resets'] for f in frames),hdmi_resets_max=max(f['hdmi_resets'] for f in frames),
        free_heap_min=min(f['free_heap_cached'] for f in frames),free_heap_max=max(f['free_heap_cached'] for f in frames),
        pc_top3_samples_only={n:100*v/sample_count for n,v in sorted(functions.items(),key=lambda item:-item[1])} if sample_count else {},
        cpu_sample_windows=len(valid),excluded_profile_windows=len(profiles)-len(valid),
        profile_bad=sum(p['bad'] for p in valid),profile_unmapped=sum(p['unmapped'] for p in valid),
        profile_samples=sample_count,invalid_partitions=summary['invalid_frame_partitions'],
        frame_number_gaps=summary['frame_number_gaps'])
    results.append(row)
    print(f"{path.name}: {len(frames)} frames {row['weighted_fps']:.2f} FPS {row['retained_wall_seconds']:.1f}s; run {means['run']:.1f} draw {means['draw']:.1f} mix {means['mix']:.1f} wait {means['wait']:.1f} q {means['qfull']:.1f} barrier {means['barrier']:.2f}; blocks {row['blocks_on_frames']}; HDMI resets {row['hdmi_resets_min']}->{row['hdmi_resets_max']}; heap {row['free_heap_min']}->{row['free_heap_max']}")
(project/'output/log-analysis-v44.json').write_text(json.dumps(results,indent=2)+'\n')
allframes=list(unique.values());wall=sum(f['wall_us'] for f in allframes)
overview=dict(captures=len(results),retained_frame_records=sum(r['frames'] for r in results),
    unique_retained_frames=len(unique),duplicate_frame_records=overlapping,
    retained_wall_seconds=wall/1e6,weighted_fps=len(unique)*1e6/wall,
    map_fingerprints=sorted({r['header']['map_fingerprint'] for r in results}),
    blocks_on_unique_frames=sum(f['blocks_on'] for f in allframes),
    invalid_frame_records=sum(r.get('invalid_partitions',0) for r in results),
    note='Deduplicated retained windows, not a controlled replay or continuous full-session average. Gaps between captures are unobserved.')
(project/'output/log-analysis-v44-overview.json').write_text(json.dumps(overview,indent=2)+'\n')
print(json.dumps(overview,indent=2))
# Private build metadata is not required for public capture analysis.
