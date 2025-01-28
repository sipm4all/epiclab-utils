#pragma once

class rwavedump
{
  
public:

  rwavedump(std::string filename);
  bool next_event() { ++current_event; return prepare(); };
  bool goto_event(int event) { current_event = event; return prepare(); };
  void rewind_events() { current_event = -1; };
  TGraph *get_graph(int group, int channel) { return graphs[group][channel]; };
  bool calibrate(std::string calibfilename);
  
private:

  TFile *file = nullptr;
  TTree *trees[2][9] = {nullptr};
  TGraph *graphs[2][9] = {nullptr};
  Long64_t n_events = -1, current_event = -1;
  int size;
  float data[1024];

  bool calib[2][9] = {false};
  float adc_calib[2][9][1024][2] = {0.};

  bool prepare();
 
};

rwavedump::rwavedump(std::string filename)
{
  std::cout << " --- opening file: " << filename << std::endl;
  file = TFile::Open(filename.c_str());
  if (!file || !file->IsOpen()) {
    std::cout << " --- could not open file: " << filename << std::endl;
    return;
  }
  for (int igr = 0; igr < 2; ++igr) {
    for (int ich = 0; ich < 9; ++ich) {
      std::string treename = Form("gr%d_ch%d", igr, ich);
      trees[igr][ich] = (TTree *)file->Get(treename.c_str());
      if (!trees[igr][ich]) continue;
      graphs[igr][ich] = new TGraph;
      if (n_events == -1) n_events = trees[igr][ich]->GetEntries();
      std::cout << " --- found data for " << treename << ": " << trees[igr][ich]->GetEntries() << " events " << std::endl;
      if (trees[igr][ich]->GetEntries() != n_events) std::cout << "     number of events mismatch " << std::endl;
    }}
}

bool
rwavedump::calibrate(std::string calibfilename)
{
  std::cout << " --- loading calibration data: " << calibfilename << std::endl;
  auto fcalib = TFile::Open(calibfilename.c_str());
  for (int igr = 0; igr < 2; ++igr) {
    for (int ich = 0; ich < 9; ++ich) {
      auto hp0 = (TH1 *)fcalib->Get(Form("hCalib_gr%d_ch%d_p0", igr, ich));
      auto hp1 = (TH1 *)fcalib->Get(Form("hCalib_gr%d_ch%d_p1", igr, ich));
      calib[igr][ich] = (hp0 && hp1);
      if (calib[igr][ich]) {
	std::cout << " --- found calibration data for " << Form("gr%d_ch%d", igr, ich) << std::endl;
	for (int i = 0; i < 1024; ++i) {
	  adc_calib[igr][ich][i][0] = hp0->GetBinContent(i + 1);
	  adc_calib[igr][ich][i][1] = hp1->GetBinContent(i + 1);
	}
      }
    }}
  
  return true;
}

bool
rwavedump::prepare()
{
  if (current_event >= n_events) return false;
  for (int igr = 0; igr < 2; ++igr) {
    for (int ich = 0; ich < 9; ++ich) {
      if (!trees[igr][ich]) continue;
      auto t = trees[igr][ich];
      auto g = graphs[igr][ich];
      g->Set(0);
      t->SetBranchAddress("size", &size);
      t->SetBranchAddress("data", &data);
      t->GetEntry(current_event);
      for (int i = 0; i < size; ++i) {
	auto valx = i;
	auto valy = calib[igr][ich] ? ( data[i] - adc_calib[igr][ich][i][0] ) / adc_calib[igr][ich][i][1] : data[i];
	g->SetPoint(i, valx, valy);
      }
      g->SetTitle(Form("gr %d ch %d: ev %lld;cell number;amplitude (%s)", igr, ich, current_event, calib[igr][ich] ? "V" : "ADC"));
    }}
  return true;
}
  
