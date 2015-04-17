#ifndef TEXTNET_LAYER_WHOLEPOOLING_LAYER_INL_HPP_
#define TEXTNET_LAYER_WHOLEPOOLING_LAYER_INL_HPP_

#include <mshadow/tensor.h>
#include "../layer.h"
#include "../op.h"

namespace textnet {
namespace layer {

template<typename xpu>
class WholePoolingLayer : public Layer<xpu>{
 public:
  WholePoolingLayer(LayerType type) { this->layer_type = type; }
  virtual ~WholePoolingLayer(void) {}
  
  virtual int BottomNodeNum() { return 1; }
  virtual int TopNodeNum() { return 1; }
  virtual int ParamNodeNum() { return 0; }
  
  virtual void Require() {
    // default value, just set the value you want
    this->defaults["pool_type"] = SettingV("max");

    // require value, set to SettingV(),
    // it will force custom to set in config

    
    Layer<xpu>::Require();
  }
  
  virtual void SetupLayer(std::map<std::string, SettingV> &setting,
                          const std::vector<Node<xpu>*> &bottom,
                          const std::vector<Node<xpu>*> &top,
                          mshadow::Random<xpu> *prnd) {
    Layer<xpu>::SetupLayer(setting, bottom, top, prnd);
    pool_type = setting["pool_type"].sVal();
  }
  
  virtual void Reshape(const std::vector<Node<xpu>*> &bottom,
                       const std::vector<Node<xpu>*> &top) {
    utils::Check(bottom.size() == BottomNodeNum(), "WholePoolingLayer:bottom size problem."); 
    utils::Check(top.size() == TopNodeNum(), "WholePoolingLayer:top size problem.");

    mshadow::Shape<4> shape_in = bottom[0]->data.shape_;
    mshadow::Shape<4> shape_out = mshadow::Shape4(shape_in[0], shape_in[1], 1, shape_in[3]);
    top[0]->Resize(shape_out, true);
    pos.Resize(shape_out, -1);

    bottom[0]->PrintShape("bottom0");
    top[0]->PrintShape("top0");
  }

  typedef mshadow::Tensor<xpu, 1> Tensor1D;
  typedef mshadow::Tensor<xpu, 2> Tensor2D;
  typedef mshadow::Tensor<xpu, 2, int> Tensor2DInt;
  typedef mshadow::Tensor<xpu, 4> Tensor4D;
  void wholeAvePooling(Tensor2D in, Tensor2D out) {
    utils::Check(in.size(1) == out.size(1) && out.size(0) == 1, "WholePoolingLayer:pooling io size error");
    out[0] = sum_rows(in);
    out /= float(in.size(0));
  }
  void wholeUnAvePooling(Tensor2D in, Tensor2D out) {
    utils::Check(in.size(1) == out.size(1) && in.size(0) == 1, "WholePoolingLayer:pooling io size error");
    out += repmat(in[0] / (float)(out.size(0)), out.size(0));
  }
  void wholeFirstPooling(Tensor2D in, Tensor2D out) {
    utils::Check(in.size(1) == out.size(1) && out.size(0) == 1, "WholePoolingLayer:pooling io size error");
    out = mshadow::expr::F<op::identity>(in.Slice(0,1));
  }
  void wholeUnFirstPooling(Tensor2D in, Tensor2D out) {
    utils::Check(in.size(1) == out.size(1) && in.size(0) == 1, "WholePoolingLayer:pooling io size error");
    out.Slice(0,1) += in;
  }
  void wholeLastPooling(Tensor2D in, Tensor2D out) {
    utils::Check(in.size(1) == out.size(1) && out.size(0) == 1, "WholePoolingLayer:pooling io size error");
    out = mshadow::expr::F<op::identity>(in.Slice(in.size(0)-1,in.size(0)));
  }
  void wholeUnLastPooling(Tensor2D in, Tensor2D out) {
    utils::Check(in.size(1) == out.size(1) && in.size(0) == 1, "WholePoolingLayer:pooling io size error");
    out.Slice(out.size(0)-1,out.size(0)) += in;
  }

  void wholeMaxPooling(Tensor2D in, Tensor2DInt pos, Tensor2D out) {
    utils::Check(in.size(1) == out.size(1) && out.size(0) == 1, "WholePoolingLayer:pooling io size error");
    utils::Check(in.size(1) == pos.size(1) && pos.size(0) == 1, "WholePoolingLayer:pooling io size error");
    out = -1000000;
    for (index_t col = 0; col < in.size(1); ++col) {
      for (index_t row = 0; row < in.size(0); ++row) {
        if (in[row][col] > out[0][col]) {
            out[0][col] = in[row][col];
            pos[0][col] = (int)(row);
        }
      }
    }
  }
  void wholeUnMaxPooling(Tensor2D in, Tensor2DInt pos, Tensor2D out) {
    utils::Check(in.size(1) == out.size(1) && in.size(0) == 1, "WholePoolingLayer:pooling io size error");
    utils::Check(in.size(1) == pos.size(1) && pos.size(0) == 1, "WholePoolingLayer:pooling io size error");
    for (index_t col = 0; col < in.size(1); ++col) {
      int row = pos[0][col];
      out[row][col] += in[0][col];
    }
  }

  void checkNan(float *p, int l) {
      for (int i = 0; i < l; ++i) {
          assert(!isnan(p[i]));
      }
  }

  virtual void Forward(const std::vector<Node<xpu>*> &bottom,
                       const std::vector<Node<xpu>*> &top) {
    using namespace mshadow::expr;
    mshadow::Tensor<xpu, 4> bottom_data = bottom[0]->data;
    mshadow::Tensor<xpu, 2> bottom_len  = bottom[0]->length;
    mshadow::Tensor<xpu, 4> top_data = top[0]->data;

    top_data = 0;
    for (index_t batch_idx = 0; batch_idx < bottom_data.size(0); ++batch_idx) {
      for (index_t seq_idx = 0; seq_idx < bottom_data.size(1); ++seq_idx) {
        int begin = 0, end = bottom_len[batch_idx][seq_idx]; 
        utils::Assert(end >= 0, "WholePoolingLayer: sequence length error.");

        if (pool_type == "max") {
            wholeMaxPooling(bottom_data[batch_idx][seq_idx].Slice(begin, end), 
                            pos[batch_idx][seq_idx], 
                            top_data[batch_idx][seq_idx]);
        } else if (pool_type == "ave") {
            wholeAvePooling(bottom_data[batch_idx][seq_idx].Slice(begin, end), 
                            top_data[batch_idx][seq_idx]);
        } else if (pool_type == "first") {
            wholeFirstPooling(bottom_data[batch_idx][seq_idx].Slice(begin, end), 
                              top_data[batch_idx][seq_idx]);
        } else if (pool_type == "last") {
            wholeLastPooling(bottom_data[batch_idx][seq_idx].Slice(begin, end), 
                             top_data[batch_idx][seq_idx]);
        } else {
            utils::Check(false, "WholePoolLayer: pool type error.");
        }
      }
    }
#if DEBUG
    checkNan(top_data.dptr_, top_data.size(0)*top_data.size(1)*top_data.size(2)*top_data.size(3));
#endif
  }
  
  virtual void Backprop(const std::vector<Node<xpu>*> &bottom,
                        const std::vector<Node<xpu>*> &top) {
    using namespace mshadow::expr;
    mshadow::Tensor<xpu, 4> top_diff = top[0]->diff;
    mshadow::Tensor<xpu, 4> bottom_data = bottom[0]->data;
    mshadow::Tensor<xpu, 2> bottom_len  = bottom[0]->length;
    mshadow::Tensor<xpu, 4> bottom_diff = bottom[0]->diff;

    for (index_t batch_idx = 0; batch_idx < bottom_data.size(0); ++batch_idx) {
      for (index_t seq_idx = 0; seq_idx < bottom_data.size(1); ++seq_idx) {
        int begin = 0, end = bottom_len[batch_idx][seq_idx]; 
        utils::Assert(end >= 0, "WholePoolingLayer: sequence length error.");

        if (this->prop_error[0]) {
          if (pool_type == "max") {
              wholeUnMaxPooling(top_diff[batch_idx][seq_idx], 
                                pos[batch_idx][seq_idx], 
                                bottom_diff[batch_idx][seq_idx].Slice(begin, end));
          } else if (pool_type == "ave") {
              wholeUnAvePooling(top_diff[batch_idx][seq_idx], 
                                bottom_diff[batch_idx][seq_idx].Slice(begin, end));
          } else if (pool_type == "first") {
              wholeUnFirstPooling(top_diff[batch_idx][seq_idx],
                                  bottom_diff[batch_idx][seq_idx].Slice(begin, end));
          } else if (pool_type == "last") {
              wholeUnLastPooling(top_diff[batch_idx][seq_idx], 
                                 bottom_diff[batch_idx][seq_idx].Slice(begin, end));
          } else {
              utils::Check(false, "WholePoolLayer: pool type error.");
          }
        }
      }
    }
  }
 protected:
  mshadow::TensorContainer<xpu, 4, int> pos;
  std::string pool_type;
};
}  // namespace layer
}  // namespace textnet
#endif  // LAYER_WHOLEPOOLING_LAYER_INL_HPP_

