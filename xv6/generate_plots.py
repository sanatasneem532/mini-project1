import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

def simulate_mlfq(total_ticks=150):
    boost_interval = 48
    slice_limits = [1, 4, 8, 16]

    # Process definitions: (pid, name, type)
    # Types: 'cpu', 'io', 'mixed'
    procs = [
        {'pid': 4, 'name': 'PID 4 (CPU-1)', 'type': 'cpu', 'prio': 0, 'slice_used': 0, 'io_remain': 0},
        {'pid': 5, 'name': 'PID 5 (CPU-2)', 'type': 'cpu', 'prio': 0, 'slice_used': 0, 'io_remain': 0},
        {'pid': 6, 'name': 'PID 6 (IO-1)',  'type': 'io',  'prio': 0, 'slice_used': 0, 'io_remain': 0},
        {'pid': 7, 'name': 'PID 7 (IO-2)',  'type': 'io',  'prio': 0, 'slice_used': 0, 'io_remain': 0},
        {'pid': 8, 'name': 'PID 8 (Mixed)', 'type': 'mixed','prio': 0, 'slice_used': 0, 'io_remain': 0},
    ]

    history = {p['pid']: {'ticks': [], 'queues': [], 'name': p['name']} for p in procs}

    for tick in range(1, total_ticks + 1):
        # Priority boost every 48 ticks
        if tick % boost_interval == 0:
            for p in procs:
                p['prio'] = 0
                p['slice_used'] = 0

        # Progress IO for sleeping procs
        for p in procs:
            if p['io_remain'] > 0:
                p['io_remain'] -= 1

        # Select highest priority runnable process
        candidate = None
        for q in range(4):
            for p in procs:
                if p['io_remain'] == 0 and p['prio'] == q:
                    candidate = p
                    break
            if candidate is not None:
                break

        if candidate is not None:
            p = candidate
            history[p['pid']]['ticks'].append(tick)
            history[p['pid']]['queues'].append(p['prio'])

            p['slice_used'] += 1

            # Behavior based on type
            if p['type'] == 'io':
                # Yields after 1 tick of CPU
                p['io_remain'] = 2
                p['slice_used'] = 0
                # Priority remains unchanged on voluntary yield
            elif p['type'] == 'mixed':
                if p['slice_used'] >= 3:
                    p['io_remain'] = 2
                    p['slice_used'] = 0
                elif p['slice_used'] >= slice_limits[p['prio']]:
                    if p['prio'] < 3:
                        p['prio'] += 1
                    p['slice_used'] = 0
            else: # CPU-bound
                if p['slice_used'] >= slice_limits[p['prio']]:
                    if p['prio'] < 3:
                        p['prio'] += 1
                    p['slice_used'] = 0

    return history

def plot_mlfq():
    history = simulate_mlfq(160)

    fig, ax = plt.subplots(figsize=(12, 6), dpi=300)

    colors = {4: '#e74c3c', 5: '#e67e22', 6: '#27ae60', 7: '#2980b9', 8: '#8e44ad'}
    markers = {4: 'o', 5: 's', 6: '^', 7: 'v', 8: 'D'}

    for pid, data in history.items():
        t = data['ticks']
        q = data['queues']
        jitter = (pid - 6) * 0.04
        q_j = [y + jitter for y in q]
        ax.plot(t, q_j, label=data['name'], color=colors[pid], marker=markers[pid],
                markersize=4, linestyle='-', alpha=0.85, linewidth=1.2)

    # Mark priority boost lines every 48 ticks
    for boost in range(48, 161, 48):
        ax.axvline(x=boost, color='#7f8c8d', linestyle='--', linewidth=1.5, alpha=0.9)
        ax.text(boost + 0.8, 0.15, f'Priority Boost\n(tick {boost})', color='#2c3e50',
                fontsize=8, fontweight='bold')

    ax.set_yticks([0, 1, 2, 3])
    ax.set_yticklabels(['Queue 0\n(Slice: 1)', 'Queue 1\n(Slice: 4)', 'Queue 2\n(Slice: 8)', 'Queue 3\n(Slice: 16)'])
    ax.invert_yaxis()

    ax.set_xlabel('Time Elapsed (ticks)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Queue ID (Priority Level)', fontsize=12, fontweight='bold')
    ax.set_title('xv6 MLFQ Scheduler Timeline & Queue Trajectory', fontsize=14, fontweight='bold', pad=15)
    ax.grid(True, linestyle=':', alpha=0.6)
    ax.legend(loc='upper right', framealpha=0.95, fontsize=9)

    # Watermark requirement: username before @ in IIIT email
    fig.text(0.5, 0.5, 'sanatasneem', fontsize=52, color='gray', alpha=0.14,
             ha='center', va='center', rotation=30, fontweight='bold')

    plt.tight_layout()
    plt.savefig('mlfq_timeline.png', bbox_inches='tight')
    plt.close()
    print("Saved mlfq_timeline.png successfully")

if __name__ == '__main__':
    plot_mlfq()
