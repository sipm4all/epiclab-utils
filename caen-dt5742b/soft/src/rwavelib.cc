#include "rwavelib.hh"

namespace dgz {

std::map<int, CAEN_DGTZ_DRS4Frequency_t> frequencies = {
  { 5000 , CAEN_DGTZ_DRS4_5GHz } ,
  { 2500 , CAEN_DGTZ_DRS4_2_5GHz } ,
  { 1000 , CAEN_DGTZ_DRS4_1GHz } ,
  {  750 , CAEN_DGTZ_DRS4_750MHz }
}; 

bool
open(digitizer_t &dgz)
{
  std::cout << " --- open digitizer " << std::endl;
  CAEN_DGTZ_ConnectionType LinkType = CAEN_DGTZ_USB;
  int LinkNum = 0;
  int ConetMode = 0;
  std::uint32_t VMEBaseAddress = 0;
  if (CAEN_DGTZ_OpenDigitizer(LinkType, LinkNum, ConetMode, VMEBaseAddress, &dgz.handle))  error("CAEN_DGTZ_OpenDigitizer");
  if (CAEN_DGTZ_GetInfo(dgz.handle, &dgz.BoardInfo))  error("CAEN_DGTZ_GetInfo");
  std::cout << " --- CAEN digitizer " << std::endl
	    << "      ModelName: " << dgz.BoardInfo.ModelName << std::endl
	    << "     FamilyCode: " << dgz.BoardInfo.FamilyCode << std::endl /** CAEN_DGTZ_XX742_FAMILY_CODE **/
	    << "       Channels: " << dgz.BoardInfo.Channels
	    << std::endl;

  dgz.open = true;
  return true;
}

bool
close(digitizer_t &dgz)
{
  if (!dgz.open) return true;
  std::cout << " --- closing digitizer, have a good day " << std::endl;
  if (CAEN_DGTZ_CloseDigitizer(dgz.handle)) error("CAEN_DGTZ_CloseDigitizer");
  return true;
}

bool
config(digitizer_t &dgz)
{
  auto handle = dgz.handle;
  auto opt = dgz.opt;

  std::cout << " --- reset digitizer " << std::endl;
  if (CAEN_DGTZ_Reset(handle)) error("CAEN_DGTZ_Reset");
  
  std::cout << " --- set record length: " << opt.record_length << std::endl;
  if (CAEN_DGTZ_SetRecordLength(handle, opt.record_length))                    error("CAEN_DGTZ_SetRecordLength");  
  ///  if (CAEN_DGTZ_SetDecimationFactor(handle, WDcfg->DecimationFactor))          error("CAEN_DGTZ_SetDecimationFactor");
  std::cout << " --- set maximum events BLT: " << opt.max_blt << std::endl;
  if (CAEN_DGTZ_SetMaxNumEventsBLT(handle, opt.max_blt))                       error("CAEN_DGTZ_SetMaxNumEventsBLT");
  if (CAEN_DGTZ_SetAcquisitionMode(handle, CAEN_DGTZ_SW_CONTROLLED))           error("CAEN_DGTZ_SetAcquisitionMode");
  if (CAEN_DGTZ_SetGroupEnableMask(handle, 0x3))                               error("CAEN_DGTZ_SetGroupEnableMask");
  std::cout << " --- set DRS4 sampling frequency: " << opt.frequency << " MHz " << std::endl;
  if (CAEN_DGTZ_SetDRS4SamplingFrequency(handle, frequencies[opt.frequency]))  error("CAEN_DGTZ_SetDRS4SamplingFrequency");

  //  if (CAEN_DGTZ_SetFastTriggerDigitizing(handle, CAEN_DGTZ_ENABLE))            error("CAEN_DGTZ_SetFastTriggerDigitizing");
  //  if (CAEN_DGTZ_SetFastTriggerDigitizing(handle, CAEN_DGTZ_DISABLE))           error("CAEN_DGTZ_SetFastTriggerDigitizing");
  //  if (CAEN_DGTZ_SetFastTriggerMode(handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY))        error("CAEN_DGTZ_SetFastTriggerMode");
  //  if (CAEN_DGTZ_SetFastTriggerMode(handle, CAEN_DGTZ_TRGMODE_DISABLED))        error("CAEN_DGTZ_SetFastTriggerMode");

  //  if (CAEN_DGTZ_SetExtTriggerInputMode(handle, CAEN_DGTZ_TRGMODE_DISABLED))    error("CAEN_DGTZ_SetExtTriggerInputMode");
  if (CAEN_DGTZ_SetExtTriggerInputMode(handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY))    error("CAEN_DGTZ_SetExtTriggerInputMode");
  //  if (CAEN_DGTZ_SetTriggerPolarity(handle, 0, CAEN_DGTZ_TriggerOnRisingEdge))  error("CAEN_DGTZ_SetTriggerPolarity 0");

  //  if (CAEN_DGTZ_SetGroupFastTriggerDCOffset(handle, 0, opt.trigger_dc))        error("CAEN_DGTZ_SetGroupFastTriggerDCOffset 0");
  //  if (CAEN_DGTZ_SetGroupFastTriggerThreshold(handle, 0, opt.trigger_thr))      error("CAEN_DGTZ_SetGroupFastTriggerThreshold 0");
  //  if (CAEN_DGTZ_SetTriggerPolarity(handle, 1, CAEN_DGTZ_TriggerOnRisingEdge))  error("CAEN_DGTZ_SetTriggerPolarity 1");
  //  if (CAEN_DGTZ_SetGroupFastTriggerDCOffset(handle, 1, opt.trigger_dc))        error("CAEN_DGTZ_SetGroupFastTriggerDCOffset 1");  
  //  if (CAEN_DGTZ_SetGroupFastTriggerThreshold(handle, 1, opt.trigger_thr))      error("CAEN_DGTZ_SetGroupFastTriggerThreshold 0");

  /** enable busy signal on GPO **/
  if (CAEN_DGTZ_WriteRegister(handle, 0x811C, 0x000D0001))                     error("CAEN_DGTZ_WriteRegister");
  if (CAEN_DGTZ_SetIOLevel(handle, CAEN_DGTZ_IOLevel_NIM))                     error("CAEN_DGTZ_SetIOLevel");

  msleep(300);
  
  /** DRS4 corrections **/

  if (CAEN_DGTZ_LoadDRS4CorrectionData(handle, frequencies[opt.frequency]))    error("CAEN_DGTZ_LoadDRS4CorrectionData");
  if (CAEN_DGTZ_EnableDRS4Correction(handle))                                  error("CAEN_DGTZ_EnableDRS4Correction");
  //  if (CAEN_DGTZ_DisableDRS4Correction(handle))                                 error("CAEN_DGTZ_DisableDRS4Correction");

  msleep(300);

  //  status(dgz);

  return true;
}

bool
start(digitizer_t &dgz)
{
  msleep(300);
  std::cout << " --- start readout " << std::endl;
  if (CAEN_DGTZ_AllocateEvent(dgz.handle, (void **)&dgz.event))                     error("CAEN_DGTZ_AllocateEvent");
  if (CAEN_DGTZ_MallocReadoutBuffer(dgz.handle, &dgz.buffer, &dgz.allocated_size))  error("CAEN_DGTZ_MallocReadoutBuffer");
  if (CAEN_DGTZ_SWStartAcquisition(dgz.handle))                                     error("CAEN_DGTZ_SWStartAcquisition");
  return true;
}

bool
stop(digitizer_t &dgz)
{
  std::cout << " --- stop readout " << std::endl;
  if (CAEN_DGTZ_SWStopAcquisition(dgz.handle))              error("CAEN_DGTZ_SWStopAcquisition");
  if (CAEN_DGTZ_FreeReadoutBuffer(&dgz.buffer))             error("CAEN_DGTZ_FreeReadoutBuffer");
  if (CAEN_DGTZ_FreeEvent(dgz.handle, (void**)&dgz.event))  error("CAEN_DGTZ_FreeEvent");
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

}
