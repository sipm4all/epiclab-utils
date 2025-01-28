#include "../lib/rwavedump.h"

TGraph *
get_waveform(std::string filename, int group, int channel, std::string calibfilename, int event)
{
  rwavedump rwd(filename);
  if (!calibfilename.empty()) rwd.calibrate(calibfilename);
  if (!rwd.goto_event(event)) return nullptr;
  return rwd.get_graph(group, channel);
}

void
draw_waveform(std::string filename, int group, int channel, std::string calibfilename = "", int event = 0)
{
  auto graph = get_waveform(filename, group, channel, calibfilename, event);
  if (!graph) {
    std::cout << " --- could not find waveform " << std::endl;
    return;
  }
  auto c = new TCanvas("c", "c", 800, 800);
  c->SetMargin(0.15, 0.15, 0.15, 0.15);
  if (calibfilename.empty()) c->DrawFrame(0., 0., 1024., 4096., ";cell number;amplitude (ADC)");
  else c->DrawFrame(0., -0.5, 1024., 0.5, ";cell number;amplitude (V)");
  graph->Draw("samelp");
}

void
average_waveform(std::string filename, int group, int channel, std::string calibfilename = "")
{
  rwavedump rwd(filename);
  if (!calibfilename.empty()) rwd.calibrate(calibfilename);
  auto graph = rwd.get_graph(group, channel);
  auto p = new TProfile("p", "p", 1024, 0, 1024, "S");
  while (rwd.next_event()) {
    for (int i = 0; i < graph->GetN(); ++i)
      p->Fill(graph->GetX()[i], graph->GetY()[i]);
  }
  p->SetMarkerStyle(6);
  p->SetMarkerColor(kAzure-3);
  p->SetFillStyle(3002);
  p->SetFillColor(kAzure-3);
  auto c = new TCanvas("c", "c", 800, 800);
  c->SetMargin(0.15, 0.15, 0.15, 0.15);
  if (calibfilename.empty()) c->DrawFrame(0., 0., 1024., 4096., ";cell number;amplitude (ADC)");
  else c->DrawFrame(0., -0.5, 1024., 0.5, ";cell number;amplitude (V)");
  p->Draw("same, p, e3");
}

