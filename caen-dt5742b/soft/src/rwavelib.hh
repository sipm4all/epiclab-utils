#pragma once

#include <iostream>
#include <cstdint>
#include <ctime>
#include <map>
#include <string>
#include <CAENDigitizer.h>

#define error(msg) std::cout << " [ERROR] " << msg << std::endl
#define log(msg) std::cout << " --- " << msg << std::endl
#define msleep(ms) usleep((ms) * 1000)

namespace dgz {

struct options_t {
  /** configuration **/
  int frequency = 5000; // 5000 2500 1000 750
  int record_length = 1024; // 1024, 520, 256 and 136
  int max_blt = 1024; // 1-1024
  int trigger_dc = 32768;
  int trigger_thr = 20934;
  int trigger_sw = 0;
  /** readout **/
  int nevents = 1;
  int readout_timeout = 1000;
  int channel_mask = 0xFFFF;
};

struct digitizer_t {
  bool open = false;
  int handle;
  CAEN_DGTZ_BoardInfo_t BoardInfo;
  CAEN_DGTZ_X742_EVENT_t *event = nullptr;
  char *buffer = nullptr;
  std::uint32_t allocated_size;
  options_t opt;
};
  
bool open(digitizer_t &dgz);
bool close(digitizer_t &dgz);
bool config(digitizer_t &dgz);
bool status(digitizer_t &dgz);
bool start(digitizer_t &dgz);
bool stop(digitizer_t &dgz);

bool test_bit(digitizer_t &dgz, uint32_t address, int bit);
#define acquisition_status(dgz) test_bit(dgz, CAEN_DGTZ_ACQ_STATUS_ADD, 2)
#define event_ready(dgz) test_bit(dgz, CAEN_DGTZ_ACQ_STATUS_ADD, 3)
#define board_ready(dgz) test_bit(dgz, CAEN_DGTZ_ACQ_STATUS_ADD, 8)

extern std::map<int, CAEN_DGTZ_DRS4Frequency_t> frequencies;

}
