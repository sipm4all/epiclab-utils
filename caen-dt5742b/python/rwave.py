#! /usr/bin/env python

import socket
import struct
import numpy as np

class rwaveclient:

    def __init__(self, host, port, verbose=True):
        self.host = host
        self.port = port
        self.socket = None
        self.verbose = verbose

        
    def __print_msg__(self, msg):
        if self.verbose:
            print(f' [CLIENT] {msg}')

        
    def __enter__(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.socket.connect((self.host, self.port))
            self.__print_msg__(f'connected to {self.host}:{self.port}')
            self.send_cmd('model')
            return self
        except ConnectionRefusedError:
            self.__print_msg__(f'failed to connect to {self.host}:{self.port}')
            self.socket = None
            return None
        except Exception as e:
            self.__print_msg__(f'connection error to {self.host}:{self.port}: {e}')
            self.socket = None
            return None

            
    def __exit__(self, exc_type, exc_value, traceback):
        if self.socket:
            self.socket.close()
            self.__print_msg__(f'connection to {self.host}:{self.port} closed')
        if exc_type:
            self.__print_msg__(f'an exception occurred: {exc_value}')
        return False

        
    def __recv_string__(self):
        raw_data = b''
        while True:
            byte = self.socket.recv(1)
            if not byte:
                return 'server closed connection'
            if byte == b'\n':
                break
            raw_data += byte
        return raw_data.decode()

    
    def send_cmd(self, msg):
        if self.socket is None:
            return
        self.socket.sendall(msg.encode())
        message = self.__recv_string__()
        if self.verbose:
            print(f' [SERVER] {message}')

        
    def download(self):
        ### receive header (4 * uint16_t)
        data_size = 4 * 2
        raw_data = self.socket.recv(data_size)
        if len(raw_data) < data_size:
            raise ValueError('received less than the expected {data_size} bytes')
        header = struct.unpack('<HHHH', raw_data)
        n_events, n_channels, record_length, frequency = header
        self.__print_msg__(f'received header: {n_events} events, {n_channels} channels, {record_length} record length, {frequency} MHz sampling')
        ### receive channels (n_channels * uint8_t)
        data_size = n_channels
        raw_data = self.socket.recv(data_size)
        if len(raw_data) < data_size:
            raise ValueError('received less than the expected {data_size} bytes')
        channels = struct.unpack('<' + n_channels * 'B', raw_data)
        self.__print_msg__(f'received channels: {channels}')
        ### receive trigger tags (n_events * 2 * uint32_t)
        data_size = n_events * 2 * 4
        raw_data = self.socket.recv(data_size)
        if len(raw_data) < data_size:
            raise ValueError('received less than the expected {data_size} bytes')
        trigger_tags = struct.unpack('<' + n_events * 2 * 'I', raw_data)
        trigger_tags = tuple(zip(trigger_tags[::2], trigger_tags[1::2]))
        self.__print_msg__(f'received trigger tags: {data_size} bytes')
        ### receive first cell indices (n_events * 2 * uint16_t)
        data_size = n_events * 2 * 2
        raw_data = self.socket.recv(data_size)
        if len(raw_data) < data_size:
            raise ValueError('received less than the expected {data_size} bytes')
        first_cells = struct.unpack('<' + n_events * 2 * 'H', raw_data)
        first_cells = tuple(zip(first_cells[::2], first_cells[1::2]))
        self.__print_msg__(f'received first_cells: {data_size} bytes')
        ### receive data
        waveform_size = record_length * 4
        event_size = n_channels * waveform_size
        data_size = n_events * event_size
        raw_data = b''
        while len(raw_data) < data_size:
            chunk = self.socket.recv(data_size - len(raw_data))
            if not chunk:  # If no data is received, connection is closed
                raise ConnectionError('server closed the connection')
            raw_data += chunk
        self.__print_msg__(f'received data: {data_size} bytes')
        ### unpack data 
        self.__print_msg__('unpacking data')
        waveform_fmt = '<' + record_length * 'f'
        waveform_struct = struct.Struct(waveform_fmt)  # Precompile for efficiency
        index = 0
        data = []
        for event in range(n_events):
            event_data = {}
            for channel in channels:
                event_data[channel] = {}
                unpacked = waveform_struct.unpack(raw_data[index:index + waveform_size])
                index += waveform_size
                waveform_data = np.array(unpacked, dtype = float)
                event_data[channel]['waveform'] = waveform_data
                event_data[channel]['trigger_tag'] = trigger_tags[event][channel // 8]
                event_data[channel]['first_cell'] = first_cells[event][channel // 8]
            data.append(event_data)
        return data
