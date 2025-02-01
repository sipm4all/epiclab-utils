#pragma once

/** send "readout [nevents]" command
    receive number of readout events
    then receive all the data and deal with it **/

namespace data {

const int max_events = 1024;
const int max_channels = 18;
const int max_length = 1024;
  
struct header_t {
  uint16_t n_events;
  uint16_t n_channels;
  uint16_t record_length;
  uint16_t frequency;
} header;

uint8_t channels[max_channels];
bool has_channel[max_channels];
   
int buffer_size;
float buffer[max_events * max_channels * max_length];


}
