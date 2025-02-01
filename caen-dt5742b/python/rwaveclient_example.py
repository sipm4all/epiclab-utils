#! /usr/bin/env python

from rwave import rwaveclient

host = 'localhost'
port = 30001

def main():
    
    with rwaveclient(host, port, verbose=True) as rwc:
        if rwc is None:
            return

        # configure
        rwc.send_cmd('sampling 750')
        rwc.send_cmd('grmask 0x1')
        rwc.send_cmd('chmask 0x0003')

        # start acquisition
        rwc.send_cmd("start")

        ### readout data
        rwc.send_cmd('swtrg 1024') # send software triggers
        rwc.send_cmd('readout')
        # download data
        rwc.send_cmd('download')
        data = rwc.download()
        
        # stop acquisition
        rwc.send_cmd('stop')

if __name__ == '__main__':
    main()
