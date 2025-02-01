# caen-dt5742b

## soft/bin/rwaveserver
The [`rwaveserver`](soft/src/rwaveserver.cc) program implements a TCP/IP server that acts as an interface between the user and the CAEN-DT5742b digitizer. 
By default the server listens on port `30001` on all interfaces. 
### Server commands
There are several commands that can be sent by a client to the server via its TCP/IP interface. To each command the server responds with a newline-terminated (`\n`) string. The client must properly reception the of the data from the server, in particular for the special commands such as `download`. A simple readout program would typically send commands to the server in the following order:
```
% ping the server
alive
model
% configuration
frequency 750
grmask 0x1
chmask 0x3
% start acquisition
start
% send some software triggers
swtrg 128
% readout and download
readout
download
% stop acquisition
stop
```
#### General commands
* `quit` : shutdown the server
* `alive` : ping the server to check if it is alive
* `model` : return the digitizer model
* `start` : start acquisition
* `stop` : stop acquisition
#### Readout commands
Readout commands can be sent only when the acquisition is running, otherwise they will be ignore.
- `readout` : readout the digitizer
- `download` : download the data
- `swtrg [ntriggers]` : send software triggers
#### Download command
The `download` command is a special command, because it also triggers the server to send data over TCP/IP.
The data are sent following this strict protocol:
1. **header** : 8 bytes fixed data size, 4 uint16_t values reporting the following information about the data being downloaded
   - `n_events` : the number of events in the data buffer
   - `n_channels` : the number of channels for each event
   - `record_length` : the length of the waveform record for each channel
   - `frequency` : the DRS4 sampling frequency in MHz
2. **channels** : `n_channels` bytes, `n_channels` uint8_t values reporting the list of channels in the events
3. **data** : `n_events * n_channels * record_length * 4` bytes, the data buffer contaning the waveforms of `n_channels` for `n_events` in `float` format
#### Configuration commands
Configuration commands can be sent only when the acquisition is not running, otherwise they will be ignore.
- `sampling [frequency]` : configure the DRS4 sampling frequency
- `grmask [mask]` : configure the group enable mask
- `chmask [mask]` : configure the channel enable mask

