#ifndef TEXTNET_LAYER_EMBEDDING_LAYER_INL_HPP_
#define TEXTNET_LAYER_EMBEDDING_LAYER_INL_HPP_

#include <iostream>
#include <fstream>
#include <sstream>
#include <set>

#include <mshadow/tensor.h>
#include "../layer.h"
#include "../op.h"

namespace textnet {
namespace layer {

template<typename xpu>
class EmbeddingLayer : public Layer<xpu>{
 public:
  EmbeddingLayer(LayerType type) { this->layer_type = type; }
  virtual ~EmbeddingLayer(void) {}
  
  virtual int BottomNodeNum() { return 1; }
  virtual int TopNodeNum() { return 1; }
  virtual int ParamNodeNum() { return 1; }
  
  virtual void SetupLayer(std::map<std::string, SettingV> &setting,
                          const std::vector<Node<xpu>*> &bottom,
                          const std::vector<Node<xpu>*> &top) {
    utils::Check(bottom.size() == BottomNodeNum(),
                  "EmbeddingLayer:bottom size problem."); 
    utils::Check(top.size() == TopNodeNum(),
                  "EmbeddingLayer:top size problem.");
                  
    embedding_file = setting["embedding_file"].s_val;
    feat_size = setting["feat_size"].i_val;
    word_count = setting["word_count"].i_val;
    doc_len = bottom[0]->data.size(3);
    doc_count = bottom[0]->data.size(1);
    nbatch = bottom[0]->data.size(0);
    
    this->params.resize(1);
    this->params[0].need_diff = false;
    this->params[0].Resize(word_count, feat_size, 1, 1);
    
    this->params[0].initializer_ =
        initializer::CreateInitializer<xpu, 4>(setting["w_filler"].i_val, *setting["w_filler"].m_val);
    this->params[0].Init();    
    
    ReadInitEmbedding();
  }
  
  void ReadInitEmbedding() {
    utils::Printf("Open embedding file: %s\n", embedding_file.c_str());	
    std::vector<std::string> lines;
    std::ifstream fin(embedding_file.c_str());
    std::string s;
    utils::Check(fin, "Open embedding file problem.");
    while (!fin.eof()) {
      std::getline(fin, s);
      lines.push_back(s);
    }
    fin.close();
    
    line_count = lines.size();
    utils::Printf("Line count in file: %d\n", line_count);

    std::istringstream iss;
    int w_idx = 0;
    for (int i = 0; i < line_count; ++i) {
      iss.clear();
      iss.seekg(0, iss.beg);
      iss.str(lines[i]);
      iss >> w_idx;
      for (int j = 0; j < feat_size; ++j) {
        iss >> this->params[0].data[w_idx][j][0][0];
      }
    }
  }
  
  virtual void Reshape(const std::vector<Node<xpu>*> &bottom,
                       const std::vector<Node<xpu>*> &top) {
    utils::Check(bottom.size() == BottomNodeNum(),
                  "EmbeddingLayer:bottom size problem."); 
    utils::Check(top.size() == TopNodeNum(),
                  "EmbeddingLayer:top size problem.");
                  
    top[0]->Resize(nbatch, doc_count, doc_len, feat_size, true);
  }
  
  virtual void Forward(const std::vector<Node<xpu>*> &bottom,
                       const std::vector<Node<xpu>*> &top) {
    using namespace mshadow::expr;
    mshadow::Tensor<xpu, 4> bottom_data = bottom[0]->data;
    mshadow::Tensor<xpu, 4> top_data = top[0]->data;
    mshadow::Tensor<xpu, 2> weight_data = this->params[0].data_d2();
    
    int w_idx = -1;
    for (int i = 0; i < nbatch; ++i) {
      for (int j = 0; j < doc_count; ++j) {
        for (int k = 0; k < doc_len; ++k) {
          w_idx = (int)bottom_data[i][j][0][k];
          if (w_idx != -1) {
            top_data[i][j][k] = F<op::identity>(weight_data[w_idx]);
          }
        }
      }
    }
  }
  
  virtual void Backprop(const std::vector<Node<xpu>*> &bottom,
                        const std::vector<Node<xpu>*> &top) {
    using namespace mshadow::expr;
    mshadow::Tensor<xpu, 4> bottom_data = bottom[0]->data;
    mshadow::Tensor<xpu, 4> top_diff = top[0]->diff;
    
    if (this->prop_grad[0]) {
      std::map<int, int> idx_map;
      int w_idx = -1;
      int inc = 0;
      for (int i = 0; i < nbatch; ++i) {
        for (int j = 0; j < doc_count; ++j) {
          for (int k = 0; k < doc_len; ++k) {
            w_idx = (int)bottom_data[i][j][0][k];
            if (w_idx != -1 && !idx_map.count(w_idx)) {
              idx_map[w_idx] = inc;
              inc++;
            }
          }
        }
      }
      
      this->params[0].diff.Resize(mshadow::Shape4(inc, feat_size, 1, 1), 0);
      this->params[0].idx.Resize(mshadow::Shape4(inc, 1, 1, 1), 0);
      mshadow::Tensor<xpu, 2> weight_diff = this->params[0].diff_d2();
      mshadow::Tensor<xpu, 1> weight_idx = this->params[0].idx_d1();
      
      for (std::map<int,int>::iterator it=idx_map.begin(); 
            it!=idx_map.end(); ++it) {
        weight_idx[it->second] = it->first;      
      }
      
      for (int i = 0; i < nbatch; ++i) {
        for (int j = 0; j < doc_count; ++j) {
          for (int k = 0; k < doc_len; ++k) {
            w_idx = (int)bottom_data[i][j][0][k];
            if (w_idx != -1) {
              weight_diff[idx_map[w_idx]] += top_diff[i][j][k];
            }
          }
        }
      }
      
    }
  }
  
 protected:
  std::string embedding_file;
  int feat_size;
  int word_count;
  int doc_len;
  int doc_count;
  int nbatch;
  int line_count;
};
}  // namespace layer
}  // namespace textnet
#endif  // LAYER_EMBEDDING_LAYER_INL_HPP_
