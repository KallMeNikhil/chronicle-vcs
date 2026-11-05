#include "engine/graphview.hpp"

#include <algorithm>

namespace chr {

std::string GraphLayout::laneRow(int star) const {
  std::string row;
  for (size_t i = 0; i < lanes_.size(); ++i) {
    row += static_cast<int>(i) == star ? '*' : (lanes_[i] >= 0 ? '|' : ' ');
    row += ' ';
  }
  return row;
}

static void trimLanes(std::vector<int>& lanes) {
  while (!lanes.empty() && lanes.back() < 0) lanes.pop_back();
}

GraphRow GraphLayout::next(int c) {
  GraphRow row;
  int col = -1;
  std::vector<size_t> closing;
  for (size_t i = 0; i < lanes_.size(); ++i) {
    if (lanes_[i] != c) continue;
    if (col < 0) col = static_cast<int>(i);
    else closing.push_back(i);
  }
  if (!closing.empty()) {
    std::string r(2 * lanes_.size(), ' ');
    for (size_t i = 0; i < lanes_.size(); ++i)
      if (lanes_[i] >= 0) r[2 * i] = '|';
    for (size_t i : closing) {
      r[2 * i] = ' ';
      r[2 * i - 1] = '/';
      lanes_[i] = -1;
    }
    while (!r.empty() && r.back() == ' ') r.pop_back();
    row.before.push_back(r);
    trimLanes(lanes_);
  }
  if (col < 0) {
    auto freeIt = std::find(lanes_.begin(), lanes_.end(), -1);
    if (freeIt != lanes_.end()) {
      col = static_cast<int>(freeIt - lanes_.begin());
      *freeIt = c;
    } else {
      col = static_cast<int>(lanes_.size());
      lanes_.push_back(c);
    }
  }
  size_t widthBefore = lanes_.size();
  const std::vector<int>& parents = dag_.node(c).parents;
  lanes_[static_cast<size_t>(col)] = parents.empty() ? -1 : parents[0];
  std::vector<size_t> opened;
  for (size_t k = 1; k < parents.size(); ++k) {
    if (std::find(lanes_.begin(), lanes_.end(), parents[k]) != lanes_.end()) continue;
    lanes_.push_back(parents[k]);
    opened.push_back(lanes_.size() - 1);
  }
  std::string prefix;
  size_t width = std::max(widthBefore, lanes_.size());
  for (size_t i = 0; i < width; ++i) {
    char ch = ' ';
    if (static_cast<int>(i) == col) ch = '*';
    else if (i < lanes_.size() && lanes_[i] >= 0 && std::find(opened.begin(), opened.end(), i) == opened.end())
      ch = '|';
    prefix += ch;
    prefix += ' ';
  }
  row.prefix = prefix;
  if (!opened.empty()) {
    std::string r(2 * lanes_.size(), ' ');
    for (size_t i = 0; i < lanes_.size(); ++i)
      if (lanes_[i] >= 0) r[2 * i] = '|';
    for (size_t i : opened) {
      r[2 * i] = ' ';
      r[2 * i - 1] = '\\';
    }
    while (!r.empty() && r.back() == ' ') r.pop_back();
    row.after.push_back(r);
  }
  trimLanes(lanes_);
  row.continuation = laneRow(-1);
  return row;
}

static std::string dotEscape(const std::string& s) {
  std::string o;
  for (char ch : s) {
    if (ch == '"' || ch == '\\') o += '\\';
    if (ch == '\n') continue;
    o += ch;
  }
  return o;
}

std::string dotGraph(const Dag& dag, const std::vector<int>& order,
                     const std::vector<std::pair<std::string, int>>& branchTips,
                     const std::vector<std::string>& summaries) {
  std::string out = "digraph chronicle {\n  rankdir=BT;\n  node [shape=ellipse];\n";
  for (size_t k = 0; k < order.size(); ++k) {
    std::string s = dag.node(order[k]).id.shortHex();
    out += "  \"" + s + "\" [label=\"" + s + "\\n" + dotEscape(summaries[k]) + "\"];\n";
  }
  for (int x : order)
    for (int p : dag.node(x).parents)
      out += "  \"" + dag.node(x).id.shortHex() + "\" -> \"" + dag.node(p).id.shortHex() + "\";\n";
  for (const auto& b : branchTips) {
    out += "  \"branch:" + dotEscape(b.first) + "\" [shape=box, label=\"" + dotEscape(b.first) + "\"];\n";
    out += "  \"branch:" + dotEscape(b.first) + "\" -> \"" + dag.node(b.second).id.shortHex() + "\";\n";
  }
  out += "}\n";
  return out;
}

}
