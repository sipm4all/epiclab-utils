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
        ### receive header
        raw_data = self.socket.recv(8)
        if len(raw_data) < 8:
            raise ValueError('received less than the expected 8 bytes')
        header = struct.unpack('<HHHH', raw_data)
        n_events, n_channels, record_length, frequency = header
        self.__print_msg__(f'received header: {n_events} events, {n_channels} channels, {record_length} record length, {frequency} MHz sampling')
        ### receive channels
        raw_data = self.socket.recv(n_channels)
        if len(raw_data) < n_channels:
            raise ValueError('received less than the expected {n_channels} bytes')
        channels = struct.unpack('<' + n_channels * 'B', raw_data)
        self.__print_msg__(f'received channels: {channels}')
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
                unpacked = waveform_struct.unpack(raw_data[index:index + waveform_size])
                index += waveform_size
                waveform_data = np.array(unpacked, dtype = float)
                event_data[channel] = waveform_data
            data.append(event_data)
        return data

