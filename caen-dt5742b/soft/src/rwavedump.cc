#include <iostream>
#include <cstdint>
#include <ctime>
#include <map>
#include <string>
#include <boost/program_options.hpp>
#include <CAENDigitizer.h>
#include "TFile.h"
#include "TTree.h"

#define error(msg) std::cout << " [ERROR] " << msg << std::endl
#define msleep(ms) usleep((ms) * 1000)

struct options_t {
  /** configuration **/
  int frequency;
  int record_length; // 1024, 520, 256 and 136
  int max_blt; // max 1023
  int trigger_dc, trigger_thr;
  int trigger_sw;
  /** readout **/
  int nevents;
  int readout_timeout;
  /** output **/
  std::string output;
  int output_mask;
};

struct digitizer_t {
  int handle;
  CAEN_DGTZ_X742_EVENT_t *event = nullptr;
  char *buffer = nullptr;
  std::uint32_t allocated_size;
  // tree stuff
  TFile *fout = nullptr;
  TTree *tout[MAX_X742_GROUP_SIZE][MAX_X742_CHANNEL_SIZE] = {nullptr};
  int size;
  float data[1024];
  options_t opt;
};


std::map<int, CAEN_DGTZ_DRS4Frequency_t> frequencies = {
  { 5000 , CAEN_DGTZ_DRS4_5GHz } ,
  { 2500 , CAEN_DGTZ_DRS4_2_5GHz } ,
  { 1000 , CAEN_DGTZ_DRS4_1GHz } ,
  {  750 , CAEN_DGTZ_DRS4_750MHz }
}; 

void process_program_options(int argc, char *argv[], options_t &opt);

bool open(digitizer_t &dgz);
bool close(digitizer_t &dgz);
bool config(digitizer_t &dgz);
bool status(digitizer_t &dgz);
bool start(digitizer_t &dgz);
bool stop(digitizer_t &dgz);
bool readout(digitizer_t &dgz);

bool test_bit(digitizer_t &dgz, uint32_t address, int bit);
#define event_ready(dgz) test_bit(dgz, CAEN_DGTZ_ACQ_STATUS_ADD, 3)

bool init_output(digitizer_t &dgz);
bool fill_output(digitizer_t &dgz);
bool write_output(digitizer_t &dgz);

int main(int argc, char *argv[])
{
  std::cout << " --- welcome to rwavedump " << std::endl;
  digitizer_t dgz;
  process_program_options(argc, argv, dgz.opt);

  if (!init_output(dgz))            /** initialize output **/
    return 1;
  
  open(dgz);                        /** open digitizer **/
  config(dgz);                      /** configure digitizer **/

  start(dgz);                       /** start acquisition **/
  readout(dgz);                     /** readout data **/
  stop(dgz);                        /** stop acquisition **/

  write_output(dgz);                /** write output data **/
  close(dgz);                       /** close digitizer **/
  
  return 0;
}

void
process_program_options(int argc, char *argv[], options_t &opt)
{
  /** process arguments **/
  namespace po = boost::program_options;
  po::options_description desc("Options");
  try {
    desc.add_options()
      ("help"             , "Print help messages")
      ("output"           , po::value<std::string>(&opt.output)->required(), "Output data filename")      
      ("nevents"          , po::value<int>(&opt.nevents)->required(), "Number of events to readout")
      ("frequency"        , po::value<int>(&opt.frequency)->required(), "DRS4 sampling frequency (MHz)")
      ("max_blt"          , po::value<int>(&opt.max_blt)->default_value(1024), "Maximum number of events for BLT transfer")
      ("record_length"    , po::value<int>(&opt.record_length)->default_value(1024), "Acquisition record length")
      ("trigger_dc"       , po::value<int>(&opt.trigger_dc)->default_value(32768), "Fast trigger DC offset")
      ("trigger_thr"      , po::value<int>(&opt.trigger_thr)->default_value(20934), "Fast trigger threshold")
      ("trigger_sw"       , po::value<int>(&opt.trigger_sw)->default_value(0), "Send software triggers")
      ("output_mask"      , po::value<int>(&opt.output_mask)->default_value(0x01FF01FF), "Output save channel mask")
      ("readout_timeout"  , po::value<int>(&opt.readout_timeout)->default_value(1000), "Readout timeout (milliseconds)")
      ;
    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    
    if (vm.count("help")) {
      std::cout << desc << std::endl;
      exit(1);
    }
  }
  catch(std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    std::cout << desc << std::endl;
    exit(1);
  }
}

bool
open(digitizer_t &dgz)
{
  std::cout << " --- open digitizer " << std::endl;
  CAEN_DGTZ_ConnectionType LinkType = CAEN_DGTZ_USB;
  int LinkNum = 0;
  int ConetMode = 0;
  std::uint32_t VMEBaseAddress = 0;
  if (CAEN_DGTZ_OpenDigitizer(LinkType, LinkNum, ConetMode, VMEBaseAddress, &dgz.handle))  error("CAEN_DGTZ_OpenDigitizer");

  CAEN_DGTZ_BoardInfo_t BoardInfo;
  if (CAEN_DGTZ_GetInfo(dgz.handle, &BoardInfo))  error("CAEN_DGTZ_GetInfo");
  std::cout << " --- CAEN digitizer " << std::endl
	    << "      ModelName: " << BoardInfo.ModelName << std::endl
	    << "     FamilyCode: " << BoardInfo.FamilyCode << std::endl /** CAEN_DGTZ_XX742_FAMILY_CODE **/
	    << "       Channels: " << BoardInfo.Channels
	    << std::endl;

  return true;
}

bool
close(digitizer_t &dgz)
{
  std::cout << " --- closing digitizer, have a good day " << std::endl;
  if (CAEN_DGTZ_CloseDigitizer(dgz.handle)) error("CAEN_DGTZ_CloseDigitizer");
  return true;
}

bool
config(digitizer_t &dgz)
{
  auto handle = dgz.handle;
  auto opt = dgz.opt;
  auto frequency = frequencies[opt.frequency];

  std::cout << " --- reset digitizer " << std::endl;
  if (CAEN_DGTZ_Reset(handle)) error("CAEN_DGTZ_Reset");
  
  std::cout << " --- set record length: " << opt.record_length << std::endl;
  if (CAEN_DGTZ_SetRecordLength(handle, opt.record_length))                    error("CAEN_DGTZ_SetRecordLength");  
  ///  if (CAEN_DGTZ_SetDecimationFactor(handle, WDcfg->DecimationFactor))          error("CAEN_DGTZ_SetDecimationFactor");
  std::cout << " --- set maximum events BLT: " << opt.max_blt << std::endl;
  if (CAEN_DGTZ_SetMaxNumEventsBLT(handle, opt.max_blt))                       error("CAEN_DGTZ_SetMaxNumEventsBLT");
  if (CAEN_DGTZ_SetAcquisitionMode(handle, CAEN_DGTZ_SW_CONTROLLED))           error("CAEN_DGTZ_SetAcquisitionMode");
  if (CAEN_DGTZ_SetGroupEnableMask(handle, 0x3))                               error("CAEN_DGTZ_SetGroupEnableMask");
  std::cout << " --- set DRS4 sampling frequency: " << frequency << std::endl;
  if (CAEN_DGTZ_SetDRS4SamplingFrequency(handle, frequency))  error("CAEN_DGTZ_SetDRS4SamplingFrequency");

  if (CAEN_DGTZ_SetFastTriggerDigitizing(handle, CAEN_DGTZ_ENABLE))            error("CAEN_DGTZ_SetFastTriggerDigitizing");
  if (CAEN_DGTZ_SetFastTriggerMode(handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY))        error("CAEN_DGTZ_SetFastTriggerMode");
  if (CAEN_DGTZ_SetExtTriggerInputMode(handle, CAEN_DGTZ_TRGMODE_DISABLED))    error("CAEN_DGTZ_SetExtTriggerInputMode");
  if (CAEN_DGTZ_SetTriggerPolarity(handle, 0, CAEN_DGTZ_TriggerOnRisingEdge))  error("CAEN_DGTZ_SetTriggerPolarity 0");
  if (CAEN_DGTZ_SetGroupFastTriggerDCOffset(handle, 0, opt.trigger_dc))        error("CAEN_DGTZ_SetGroupFastTriggerDCOffset 0");
  if (CAEN_DGTZ_SetGroupFastTriggerThreshold(handle, 0, opt.trigger_thr))      error("CAEN_DGTZ_SetGroupFastTriggerThreshold 0");
  if (CAEN_DGTZ_SetTriggerPolarity(handle, 1, CAEN_DGTZ_TriggerOnRisingEdge))  error("CAEN_DGTZ_SetTriggerPolarity 1");
  if (CAEN_DGTZ_SetGroupFastTriggerDCOffset(handle, 1, opt.trigger_dc))        error("CAEN_DGTZ_SetGroupFastTriggerDCOffset 1");  
  if (CAEN_DGTZ_SetGroupFastTriggerThreshold(handle, 1, opt.trigger_thr))      error("CAEN_DGTZ_SetGroupFastTriggerThreshold 0");

  /** enable busy signal on GPO **/
  if (CAEN_DGTZ_WriteRegister(handle, 0x811C, 0x000D0001))                     error("CAEN_DGTZ_WriteRegister");

  msleep(300);
  
  /** DRS4 corrections **/

  //  if (CAEN_DGTZ_LoadDRS4CorrectionData(handle, frequencies[opt.frequency]))    error("CAEN_DGTZ_LoadDRS4CorrectionData");
  //  if (CAEN_DGTZ_EnableDRS4Correction(handle))                                  error("CAEN_DGTZ_EnableDRS4Correction");
  if (CAEN_DGTZ_DisableDRS4Correction(handle))                                 error("CAEN_DGTZ_DisableDRS4Correction");

  //  status(dgz);

  return true;
}

bool
start(digitizer_t &dgz)
{
  msleep(300);
  if (CAEN_DGTZ_AllocateEvent(dgz.handle, (void **)&dgz.event))                     error("CAEN_DGTZ_AllocateEvent");
  if (CAEN_DGTZ_MallocReadoutBuffer(dgz.handle, &dgz.buffer, &dgz.allocated_size))  error("CAEN_DGTZ_MallocReadoutBuffer");
  if (CAEN_DGTZ_SWStartAcquisition(dgz.handle))                                     error("CAEN_DGTZ_SWStartAcquisition");
  return true;
}

bool
stop(digitizer_t &dgz)
{
  if (CAEN_DGTZ_SWStopAcquisition(dgz.handle))              error("CAEN_DGTZ_SWStopAcquisition");
  if (CAEN_DGTZ_FreeReadoutBuffer(&dgz.buffer))             error("CAEN_DGTZ_FreeReadoutBuffer");
  if (CAEN_DGTZ_FreeEvent(dgz.handle, (void**)&dgz.event))  error("CAEN_DGTZ_FreeEvent");
  return true;
}

bool
readout(digitizer_t &dgz)
{
  auto opt = dgz.opt;
  
  std::uint32_t buffer_size = 0, num_events = 0, tot_events = 0;

  std::cout << " --- readout data " << std::endl;
  while (tot_events < opt.nevents) {
  
    /** send software triggers **/
    for (int iswtrg = 0; iswtrg < opt.trigger_sw; ++iswtrg) {
      if (CAEN_DGTZ_SendSWtrigger(dgz.handle))  error("CAEN_DGTZ_SendSWtrigger");
      msleep(1);
    }

    /** poll for event ready **/
    bool timeout = true;
    for (int ipoll = 0; ipoll < opt.readout_timeout; ++ipoll) {
      msleep(1);
      if (!event_ready(dgz)) continue;
      timeout = false;
      break;
    }

    /** check readout timeout **/
    if (timeout) {
      std::cout << " --- readout timeout " << std::endl;
      break;
    }
  
    /** data available to be read **/
    if (CAEN_DGTZ_ReadData(dgz.handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, dgz.buffer, &buffer_size))  error("CAEN_DGTZ_ReadData");
    if (CAEN_DGTZ_GetNumEvents(dgz.handle, dgz.buffer, buffer_size, &num_events))  error("CAEN_DGTZ_GetNumEvents");
    std::cout << " --- readout " << num_events << " events " << std::endl;

    /** decode events and write to file **/
    CAEN_DGTZ_EventInfo_t event_info;
    char *event_ptr = nullptr;
    for (int iev = 0; iev < num_events && tot_events < opt.nevents; ++iev) {
      if (CAEN_DGTZ_GetEventInfo(dgz.handle, dgz.buffer, buffer_size, iev, &event_info, &event_ptr))  error("CAEN_DGTZ_GetEventInfo");
      if (CAEN_DGTZ_DecodeEvent(dgz.handle, event_ptr, (void **)&dgz.event))  error("CAEN_DGTZ_DecodeEvent");
      /** fill output tree **/
      fill_output(dgz);
      ++tot_events;
    }
    
  }

  std::cout << " --- readout done: collected " << tot_events << " events " << std::endl;
  
  return true;
}

bool
status(digitizer_t &dgz)
{
  uint32_t status = 0;
  CAEN_DGTZ_ReadRegister(dgz.handle, CAEN_DGTZ_ACQ_STATUS_ADD, &status);
  std::cout << " --- board status after configure " << status << std::endl;
  usleep(1000);
  CAEN_DGTZ_ReadRegister(dgz.handle, CAEN_DGTZ_ACQ_STATUS_ADD, &status);
  std::cout << " --- board status after configure " << status << std::endl;

  if(!(status & (1 << 7)))  error("board error detected: PLL not locked");

  return true;
}

bool
test_bit(digitizer_t &dgz, uint32_t address, int bit)
{
  uint32_t status = 0;
  CAEN_DGTZ_ReadRegister(dgz.handle, address, &status);
  return (status & (1 << bit));
}

bool
init_output(digitizer_t &dgz)
{
  auto filename = dgz.opt.output;
  dgz.fout = TFile::Open(filename.c_str(), "RECREATE");
  if (!dgz.fout || !dgz.fout->IsOpen()) {
    std::cout << " --- cannot open output file: " << filename << std::endl;
    return false;
  }
  return true;
}

bool
write_output(digitizer_t &dgz)
{
  auto filename = dgz.opt.output;
  if (!dgz.fout || !dgz.fout->IsOpen()) return false;
  dgz.fout->cd();
  std::cout << " --- writing output: " << filename << std::endl;
  for (int igr = 0; igr < MAX_X742_GROUP_SIZE; ++igr) {
    for (int ich = 0; ich < MAX_X742_CHANNEL_SIZE; ++ich) {
      auto tout = dgz.tout[igr][ich];
      if (!tout) continue;
      std::cout << " --- writing output tree: " << tout->GetName() << std::endl;
      tout->Write();
    }
  }
  dgz.fout->Close();
  return true;
}

bool
fill_output(digitizer_t &dgz)
{
  auto output_mask = dgz.opt.output_mask;
  /** loop over groups **/
  for (int igr = 0; igr < MAX_X742_GROUP_SIZE; ++igr) {
    if (dgz.event->GrPresent[igr] == 0) continue;
    auto mask = output_mask >> (16 * igr);
    /** loop over channels **/
    for (int ich = 0; ich < MAX_X742_CHANNEL_SIZE; ++ich) {
      if (!(mask & 1 << ich)) continue;
      /** create tree the first time **/
      if (!dgz.tout[igr][ich]) {
	std::string tname = "gr" + std::to_string(igr) + "_ch" + std::to_string(ich);
	dgz.tout[igr][ich] = new TTree(tname.c_str(), "rwavedump");
	dgz.tout[igr][ich]->Branch("size", &dgz.size, "size/I");
	dgz.tout[igr][ich]->Branch("data", &dgz.data, "data[size]/F");
      }
      /** store size and data **/
      dgz.size = dgz.event->DataGroup[igr].ChSize[ich];
      for (int i = 0; i < dgz.size; ++i)
	dgz.data[i] = dgz.event->DataGroup[igr].DataChannel[ich][i];
      /** fill the tree **/
      dgz.tout[igr][ich]->Fill();
    }
  }
  return true;
}
