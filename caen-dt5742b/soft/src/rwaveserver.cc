#include <iostream>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 30001
#define BUFFER_SIZE 1024

#include "rwavelib.hh"
#include "rwavedata.hh"
#include <vector>
#include <sstream>
#include <algorithm>
#include <regex>
#include <iomanip>

void message(int fd, std::string msg) {
  log(msg);
  msg = msg + " \n";
  send(fd, msg.c_str(), msg.size(), 0);
}

int server_fd;
dgz::digitizer_t DGZ;

void handle_signal(int signal) {
  log("CTRL+C interrupt");
  /** close digitizer **/
  dgz::close(DGZ);
  /** close server socket **/
  log("server is shutting down, have a good day");
  close(server_fd);
  exit(0);
}

void process_command(int client_fd, const std::string &str);
bool is_valid_hex(const std::string& str);
bool is_valid_int(const std::string& str);

bool fill_buffer(dgz::digitizer_t &dgz);

int main() {
  struct sockaddr_in address;
  socklen_t addr_len = sizeof(address);
  char buffer[BUFFER_SIZE] = {0};
  std::string mystring;
  
  /** handle SIGINT (Ctrl+C) **/
  signal(SIGINT, handle_signal);
  
  /** create socket **/
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    error("socket creation failed");
    return 1;
  }
  
  /** configure server address structure **/
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);
  
  /** bind socket **/
  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    error("bind failed");
    close(server_fd);
    return 1;
  }
  
  /** listen for incoming connections **/
  if (listen(server_fd, 3) < 0) {
    error("listen failed");
    close(server_fd);
    return 1;
  }
  
  mystring = "server listening on port " + std::to_string(PORT);
  log(mystring);
  
  /** open digitizer **/
  dgz::open(DGZ);
  
  /** configure digitizer **/
  dgz::config(DGZ);
  
  while (true) {
    int client_fd = accept(server_fd, (struct sockaddr*)&address, &addr_len);
    if (client_fd < 0) {
      error("accept failed");
      continue;
    }
    
    log("client connected");
    
    /** receive data **/
    while (true) {
      ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
      if (bytes_received <= 0) {
	log("client disconnected");
	break;
      }
      
      buffer[bytes_received] = '\0'; // Null-terminate received data
      
      /** trim newline characters for proper comparison **/
      std::string received_str(buffer);
      received_str.erase(received_str.find_last_not_of("\r\n") + 1);
      mystring = "received message from client: " + received_str;
      log(mystring);
      
      process_command(client_fd, received_str);
      
    }
    
    /** close clien socket **/
    close(client_fd);
  }
  
  /** close digitizer **/
  dgz::close(DGZ);
  
  /** close server socket **/
  close(server_fd);
  
  return 0;
}

bool
is_valid_hex(const std::string& str) {
  if (str.empty()) return false;
  std::regex hexPattern(R"(^(0[xX])?[A-Fa-f0-9]+$)");
  return std::regex_match(str, hexPattern);
}

bool is_valid_int(const std::string& str) {
  std::regex intPattern(R"(^[+-]?\d+$)");
  return !str.empty() && std::regex_match(str, intPattern);
}

void
process_command(int client_fd, const std::string &str)
{
  std::string mystring;
  
  /** quit **/
  if (str.find("quit") == 0) {
    dgz::close(DGZ);
    mystring = "server is shutting down, have a good day";
    message(client_fd, mystring);
    close(client_fd);
    close(server_fd);
    exit(0);
  }
  
  /** alive **/
  if (str.find("alive") == 0) {
    mystring = "server is alive";
    message(client_fd, mystring);
    return;
  }

  /** model **/
  if (str.find("model") == 0) {
    mystring = "model name: " + std::string(DGZ.BoardInfo.ModelName);
    message(client_fd, mystring);
    return;
  }

  /** start **/
  if (str.find("start") == 0) {
    if (!dgz::board_ready(DGZ)) {
      mystring = "the board is not ready to start acquisition";
      message(client_fd, mystring);
      return;
    }
    if (dgz::acquisition_status(DGZ)) {
      mystring = "acquisition is already running";
      message(client_fd, mystring);
      return;
    }
    if (!dgz::start(DGZ)) {
      mystring = "[ERROR] problems starting acquisition";
      message(client_fd, mystring);
      return;
    }
    mystring = "acquisition started";
    message(client_fd, mystring);
    return;
  }

  /** stop **/
  if (str.find("stop") == 0) {
    if (!dgz::acquisition_status(DGZ)) {
      mystring = "acquisition is not running";
      message(client_fd, mystring);
      return;
    }
    if (!dgz::stop(DGZ)) {
      mystring = "[ERROR] problems stopping acquisition";
      message(client_fd, mystring);
      return;
    }
    mystring = "acquisition stopped";
    message(client_fd, mystring);
    return;
  }

  /**
   ** swtrg - software trigger
   **/
  
  if (str.find("swtrg") == 0) {
    if (!dgz::acquisition_status(DGZ)) {
      mystring = "cannot send soft triggers, acquisition is not running";
      message(client_fd, mystring);
      return;
    }
    std::stringstream ss(str);
    std::string word;
    std::vector<std::string> words;
    while (ss >> word) words.push_back(word);
    if (words.size() != 2) {
      mystring = "[ERROR] sw_trigger command requires one argument: [ntriggers]";
      message(client_fd, mystring);
      return;
    }
    const std::string& astr = words[1];
    if (!is_valid_int(astr)) {
      mystring = "[ERROR] invalid sw_trigger argument, not a valid integer: " + astr;
      message(client_fd, mystring);
      return;
    }
    int ntriggers = std::stoi(astr);

    log("send software triggers");
    for (int itrg = 0; itrg < ntriggers; ++itrg) {
      if (CAEN_DGTZ_SendSWtrigger(DGZ.handle)) {
	mystring = "[ERROR] CAEN_DGTZ_SendSWtrigger";
	message(client_fd, mystring);
	return;
      }
      msleep(1);
    }

    mystring = "software triggers sent: " + astr;
    message(client_fd, mystring);
    return;
  }
  
  /**
   ** readout
   **/
  
  if (str.find("readout") == 0) {
    if (!dgz::acquisition_status(DGZ)) {
      mystring = "cannot readout data, acquisition is not running";
      message(client_fd, mystring);
      return;
    }

    /** reset header **/
    data::header.n_events = 0;
    data::header.record_length = DGZ.opt.record_length;
    data::header.frequency = DGZ.opt.frequency;
    data::header.n_channels = 0;
    
    log("poll for event ready");
    for (int ipoll = 0; ipoll < DGZ.opt.readout_timeout; ++ipoll) {
      msleep(1);
      if (!dgz::event_ready(DGZ)) continue;
      break;
    }
    if (!dgz::event_ready(DGZ)) {
      mystring = "readout timeout";
      message(client_fd, mystring);
      return;
    }
  
    log("data available to be read");
    std::uint32_t buffer_size = 0;
    if (CAEN_DGTZ_ReadData(DGZ.handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, DGZ.buffer, &buffer_size)) {
      mystring = "[ERROR] CAEN_DGTZ_ReadData";
      message(client_fd, mystring);
      return;
    }
    std::uint32_t num_events = 0;
    if (CAEN_DGTZ_GetNumEvents(DGZ.handle, DGZ.buffer, buffer_size, &num_events)) {
      mystring = "[ERROR] CAEN_DGTZ_GetNumEvents";
      message(client_fd, mystring);
      return;
    }
    mystring = "readout " + std::to_string(num_events) + " events";
    log(mystring);

    log("decode events");
    CAEN_DGTZ_EventInfo_t event_info;
    char *event_ptr = nullptr;
    data::buffer_size = 0;
    std::fill(std::begin(data::has_channel), std::end(data::has_channel), false);
    for (int iev = 0; iev < num_events; ++iev) {
      if (CAEN_DGTZ_GetEventInfo(DGZ.handle, DGZ.buffer, buffer_size, iev, &event_info, &event_ptr)) {
	mystring = "[ERROR] CAEN_DGTZ_GetEventInfo";
	message(client_fd, mystring);
	return;
      }
      if (CAEN_DGTZ_DecodeEvent(DGZ.handle, event_ptr, (void **)&DGZ.event)) {
	mystring = "[ERROR] CAEN_DGTZ_DecodeEvent";
	message(client_fd, mystring);
	return;
      }
      fill_buffer(DGZ);
    }
    mystring = "readout completed: " + std::to_string(num_events) + " events";
    message(client_fd, mystring);

    /** prepare data **/
    data::header.n_events = num_events;
    for (int ich = 0; ich < data::max_channels; ++ich)
      if (data::has_channel[ich]) data::channels[data::header.n_channels++] = ich;

    return;
    
  }
  
  /**
   ** download
   **/
  
  if (str.find("download") == 0) {
    int header_size = sizeof(data::header);
    int channels_size = data::header.n_channels * sizeof(uint8_t);
    int data_size = data::buffer_size * sizeof(float);
    mystring = "sending header,channels,data: " +
      std::to_string(header_size) + ","  +
      std::to_string(channels_size) + "," +
      std::to_string(data_size) + " bytes";
    message(client_fd, mystring);
    
    send(client_fd, &data::header, header_size, 0);
    send(client_fd, &data::channels, channels_size, 0);    
    send(client_fd, data::buffer, data_size, 0);

    return;
  }

  /** sampling [MHz] **/
  if (str.find("sampling") == 0) {
    if (dgz::acquisition_status(DGZ)) {
      mystring = "cannot change configuration, acquisition is running";
      message(client_fd, mystring);
      return;
    }
    // Step 2: Split the string into words
    std::stringstream ss(str);
    std::string word;
    std::vector<std::string> words;
    while (ss >> word) words.push_back(word);
    if (words.size() != 2) {
      mystring = "[ERROR] \'sampling\' command requires one argument: \'frequency (MHz)\'";
      message(client_fd, mystring);
      return;
    }
    const std::string& astr = words[1];
    if (!is_valid_int(astr)) {
      mystring = "[ERROR] invalid \'sampling\' argument, not a valid integer: " + astr;
      message(client_fd, mystring);
      return;
    }
    int frequency = std::stoi(astr);
    if (frequency != 5000 && frequency != 2500 && frequency != 1000 && frequency != 750) {
      mystring = " [ERROR] invalid \'sampling\' argument, not a valid value [5000, 2500, 1000, 750]: " + astr;
      message(client_fd, mystring);
      return;
    }
    if (CAEN_DGTZ_SetDRS4SamplingFrequency(DGZ.handle, dgz::frequencies[frequency])) {
      mystring = "[ERROR] CAEN_DGTZ_SetDRS4SamplingFrequency";
      message(client_fd, mystring);
      return;
    }
    DGZ.opt.frequency = frequency;
    mystring = "sampling frequency configured: " + astr;
    message(client_fd, mystring);
    return;      
  }
  
  /**
   ** grmask [mask] -- configure group mask
   **/
  
  if (str.find("grmask") == 0) {
    if (dgz::acquisition_status(DGZ)) {
      mystring = "cannot change configuration, acquisition is running";
      message(client_fd, mystring);
      return;
    }
    // Step 2: Split the string into words
    std::stringstream ss(str);
    std::string word;
    std::vector<std::string> words;
    while (ss >> word) words.push_back(word);
    if (words.size() != 2) {
      mystring = "[ERROR] \'grmask\' command requires one argument: \'mask\'";
      message(client_fd, mystring);
      return;
    }
    const std::string& astr = words[1];
    int mask = 0x0;
    if (is_valid_int(astr)) mask = std::stoi(astr);
    else if (is_valid_hex(astr)) {
      std::string hstr = astr;
      if (hstr.substr(0, 2) == "0x" || hstr.substr(0, 2) == "0X") hstr = hstr.substr(2);
      mask = std::stoi(hstr, nullptr, 16);
    }
    else {
      mystring = "[ERROR] invalid \'grmask\' argument, not a valid integer/hex: " + astr;
      message(client_fd, mystring);
      return;
    }
    if (mask < 0x0 || mask > 0x3) {
      mystring = "[ERROR] invalid \'grmask\' argument, not a valid value [0-3]: " + astr;
      message(client_fd, mystring);
      return;
    }
    if (CAEN_DGTZ_SetGroupEnableMask(DGZ.handle, mask)) {
      mystring = "[ERROR] CAEN_DGTZ_SetGroupEnableMask";
      message(client_fd, mystring);
      return;
    }
    mystring = "group enable mask configured: " + astr;
    message(client_fd, mystring);
    return;      
  }
  
  /**
   ** chmask [mask] -- configure channel mask
   **/
  
  if (str.find("chmask") == 0) {
    if (dgz::acquisition_status(DGZ)) {
      mystring = "cannot change configuration, acquisition is running";
      message(client_fd, mystring);
      return;
    }
    // Step 2: Split the string into words
    std::stringstream ss(str);
    std::string word;
    std::vector<std::string> words;
    while (ss >> word) words.push_back(word);
    if (words.size() != 2) {
      mystring = "[ERROR] \'chmask\' command requires one argument: \'mask\'";
      message(client_fd, mystring);
      return;
    }
    const std::string& astr = words[1];
    int mask = 0x0;
    if (is_valid_int(astr)) mask = std::stoi(astr);
    else if (is_valid_hex(astr)) {
      std::string hstr = astr;
      if (hstr.substr(0, 2) == "0x" || hstr.substr(0, 2) == "0X") hstr = hstr.substr(2);
      mask = std::stoi(hstr, nullptr, 16);
    }
    else {
      mystring = "[ERROR] invalid \'chmask\' argument, not a valid integer/hex: " + astr;
      message(client_fd, mystring);
      return;
    }
    if (mask < 0 || mask > 0xff) {
      mystring = "[ERROR] invalid \'chmask\' argument, not a valid value [0-65535]: " + astr;
      message(client_fd, mystring);
      return;
    }
    mystring = "channel mask configured: " + astr;
    DGZ.opt.channel_mask = mask;
    message(client_fd, mystring);
    return;      
  }
  
  mystring = "[ERROR] unknown command: " + str;
  message(client_fd, mystring);
  return;
}

bool
fill_buffer(dgz::digitizer_t &dgz)
{
  auto channel_mask = DGZ.opt.channel_mask;
  /** loop over groups **/
  for (int igr = 0; igr < 2; ++igr) {
    if (dgz.event->GrPresent[igr] == 0) continue;
    auto mask = channel_mask >> (8 * igr);
    /** loop over channels **/
    for (int ich = 0; ich < 8; ++ich) {
      if (!(mask & 1 << ich)) continue;
      auto size = dgz.event->DataGroup[igr].ChSize[ich];
      uint8_t ch = ich + igr * 9;
      data::has_channel[ch] = true;
      for (int i = 0; i < size; ++i)
	data::buffer[data::buffer_size + i] = dgz.event->DataGroup[igr].DataChannel[ich][i];
      data::buffer_size += size;
    }
  }
  return true;
}
