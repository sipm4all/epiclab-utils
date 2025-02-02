#! /usr/bin/env python

from rwave import rwaveclient
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation

host = 'localhost'
port = 30001

paused = False
current_frame = 0

frequency = 750
channels = [0, 1, 12]

def collect_data():

    grmask = 0
    chmask = 0
    for ch in channels:
        grmask |= 1 << ch // 8 
        chmask |= 1 << ch
    
    data = None
    with rwaveclient(host, port, verbose=True) as rwc:
        if rwc is None:
            return
        rwc.send_cmd(f'sampling {frequency}')
        rwc.send_cmd(f'grmask {grmask}')
        rwc.send_cmd(f'chmask {chmask}')
        rwc.send_cmd("start")
        rwc.send_cmd('swtrg 1024')
        rwc.send_cmd('readout')
        rwc.send_cmd('download')
        data = rwc.download()
        rwc.send_cmd('stop')
    return data


def update(frame, data, lines, ax):
    global current_frame
    current_frame = frame
    for ch in channels:
        lines[ch].set_ydata(data[current_frame][ch])
    ax.set_title(f'event {current_frame}')
    return list(lines.values())


def toggle_pause(event, ani):
    global paused
    if event.key == ' ':
        if paused:
            ani.event_source.start()
        else:
            ani.event_source.stop()
        paused = not paused

        
def next_frame(event, fig, data, lines, ax):
    global current_frame
    if event.key == 'n':
        current_frame += 1
        update(current_frame, data, lines, ax)
        fig.canvas.draw()
        fig.canvas.flush_events()

        
def main():

    data = collect_data()

    fig, ax = plt.subplots()
    x_data = np.arange(1024)
    y_data = np.zeros(1024)
    lines = {}
    for ch in channels:
        lines[ch], = ax.plot(x_data, y_data, label=f'ch-{ch}')

    ax.set_ylim(0, 4096)
    ax.set_xlabel("cell")
    ax.set_ylabel("ADC")
    ax.set_title(f'event {current_frame}')
    ax.grid()
    ax.legend()

    ani = animation.FuncAnimation(fig, update, fargs=(data, lines, ax), frames=len(data), interval=100, blit=False, repeat=False)
    ani.event_source.stop()
    fig.canvas.mpl_connect('key_press_event', lambda event: toggle_pause(event, ani))
    fig.canvas.mpl_connect('key_press_event', lambda event: next_frame(event, fig, data, lines, ax))
        
    plt.show()

        
if __name__ == '__main__':
    main()
