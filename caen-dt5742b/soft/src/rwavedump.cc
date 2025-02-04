#include <boost/program_options.hpp>
#include "rwavelib.hh"
#include "TFile.h"
#include "TTree.h"

using namespace dgz;

// tree stuff
struct output_t {
  std::string output;
  TFile *fout = nullptr;
  TTree *tout[MAX_X742_GROUP_SIZE][MAX_X742_CHANNEL_SIZE] = {nullptr};
  int size;
  uint32_t ttag; // trigger time tag
  uint16_t strt; // start index cell
  float data[1024];
};

void process_program_options(int argc, char *argv[], options_t &opt, output_t &out);

bool readout(digitizer_t &dgz, output_t &out);

bool init_output(output_t &out);
bool fill_output(digitizer_t &dgz, output_t &out);
bool write_output(output_t &out);

int main(int argc, char *argv[])
{
  std::cout << " --- welcome to rwavedump " << std::endl;
  digitizer_t dgz;
  output_t out;
  process_program_options(argc, argv, dgz.opt, out);

  if (!init_output(out))            /** initialize output **/
    return 1;
  
  open(dgz);                        /** open digitizer **/
  config(dgz);                      /** configure digitizer **/

  start(dgz);                       /** start acquisition **/
  readout(dgz, out);                /** readout data **/
  stop(dgz);                        /** stop acquisition **/

  write_output(out);                /** write output data **/
  close(dgz);                       /** close digitizer **/
  
  return 0;
}

void
process_program_options(int argc, char *argv[], options_t &opt, output_t &out)
{
  /** process arguments **/
  namespace po = boost::program_options;
  po::options_description desc("Options");
  try {
    desc.add_options()
      ("help"             , "Print help messages")
      ("output"           , po::value<std::string>(&out.output)->required(), "Output data filename")      
      ("nevents"          , po::value<int>(&opt.nevents)->required(), "Number of events to readout")
      ("frequency"        , po::value<int>(&opt.frequency)->required(), "DRS4 sampling frequency (MHz)")
      ("max_blt"          , po::value<int>(&opt.max_blt)->default_value(1024), "Maximum number of events for BLT transfer")
      ("record_length"    , po::value<int>(&opt.record_length)->default_value(1024), "Acquisition record length")
      ("trigger_dc"       , po::value<int>(&opt.trigger_dc)->default_value(32768), "Fast trigger DC offset")
      ("trigger_thr"      , po::value<int>(&opt.trigger_thr)->default_value(20934), "Fast trigger threshold")
      ("trigger_sw"       , po::value<int>(&opt.trigger_sw)->default_value(0), "Send software triggers")
      ("trigger_sw_usleep" , po::value<int>(&opt.trigger_sw_usleep)->default_value(1000), "Delay between software triggers (microseconds)")
      ("channel_mask"     , po::value<int>(&opt.channel_mask)->default_value(0x01FF01FF), "Output save channel mask")
      ("readout_msleep"   , po::value<int>(&opt.readout_msleep)->default_value(1), "Readout sleep (ms)")
      ("readout_timeout"  , po::value<int>(&opt.readout_timeout)->default_value(1000), "Readout timeout (ms)")
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
readout(digitizer_t &dgz, output_t &out)
{

  auto opt = dgz.opt;
  std::uint32_t buffer_size = 0, num_events = 0, tot_events = 0;

  std::cout << " --- readout data " << std::endl;
  while (tot_events < opt.nevents) {
  
    /** send software triggers **/
    for (int iswtrg = 0; iswtrg < opt.trigger_sw; ++iswtrg) {
      if (CAEN_DGTZ_SendSWtrigger(dgz.handle))  error("CAEN_DGTZ_SendSWtrigger");
      usleep(opt.trigger_sw_usleep);
    }

    /** poll for event ready **/
    bool timeout = true;
    for (int ipoll = 0; ipoll < opt.readout_timeout; ipoll += opt.readout_msleep) {
      msleep(opt.readout_msleep);
      if (!event_ready(dgz)) continue;
      //      if (!event_full(dgz)) continue;
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
      fill_output(dgz, out);
      ++tot_events;
    }
    
  }

  std::cout << " --- readout done: collected " << tot_events << " events " << std::endl;
  
  return true;
}

bool
init_output(output_t &out)
{
  auto filename = out.output;
  out.fout = TFile::Open(filename.c_str(), "RECREATE");
  if (!out.fout || !out.fout->IsOpen()) {
    std::cout << " --- cannot open output file: " << filename << std::endl;
    return false;
  }
  return true;
}

bool
write_output(output_t &out)
{
  auto filename = out.output;
  if (!out.fout || !out.fout->IsOpen()) return false;
  out.fout->cd();
  std::cout << " --- writing output: " << filename << std::endl;
  for (int igr = 0; igr < MAX_X742_GROUP_SIZE; ++igr) {
    for (int ich = 0; ich < MAX_X742_CHANNEL_SIZE; ++ich) {
      auto tout = out.tout[igr][ich];
      if (!tout) continue;
      std::cout << " --- writing output tree: " << tout->GetName() << std::endl;
      tout->Write();
    }
  }
  out.fout->Close();
  return true;
}

bool
fill_output(digitizer_t &dgz, output_t &out)
{
  auto opt = dgz.opt;
  auto channel_mask = opt.channel_mask;
  /** loop over groups **/
  for (int igr = 0; igr < MAX_X742_GROUP_SIZE; ++igr) {
    if (dgz.event->GrPresent[igr] == 0) continue;
    auto mask = channel_mask >> (16 * igr);
    /** loop over channels **/
    for (int ich = 0; ich < MAX_X742_CHANNEL_SIZE; ++ich) {
      if (!(mask & 1 << ich)) continue;
      /** create tree the first time **/
      if (!out.tout[igr][ich]) {
	std::string tname = "gr" + std::to_string(igr) + "_ch" + std::to_string(ich);
	out.tout[igr][ich] = new TTree(tname.c_str(), "rwavedump");
	out.tout[igr][ich]->Branch("size", &out.size, "size/I");
	out.tout[igr][ich]->Branch("ttag", &out.ttag, "ttag/i");
	out.tout[igr][ich]->Branch("strt", &out.strt, "strt/s");
	out.tout[igr][ich]->Branch("data", &out.data, "data[size]/F");
      }
      /** store size and data **/
      out.size = dgz.event->DataGroup[igr].ChSize[ich];
      out.ttag = dgz.event->DataGroup[igr].TriggerTimeTag;
      out.strt = dgz.event->DataGroup[igr].StartIndexCell;
      for (int i = 0; i < out.size; ++i)
	out.data[i] = dgz.event->DataGroup[igr].DataChannel[ich][i];
      /** fill the tree **/
      out.tout[igr][ich]->Fill();
    }
  }
  return true;
}
