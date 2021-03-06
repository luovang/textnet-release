#ifndef TEXTNET_LAYER_ELU_LAYER_INL_HPP_
#define TEXTNET_LAYER_ELU_LAYER_INL_HPP_

#include <mshadow/tensor.h>
#include "../layer.h"
#include "../op.h"

namespace textnet {
namespace layer {

template<typename xpu>
class ELULayer : public Layer<xpu>{
 public:
  ELULayer(LayerType type) { this->layer_type = type; }
  virtual ~ELULayer(void) {}
  
  virtual int BottomNodeNum() { return 1; }
  virtual int TopNodeNum() { return 1; }
  virtual int ParamNodeNum() { return 0; }
  
  virtual void Require() {
    // default value, just set the value you want
    this->defaults["b"] = SettingV();
    
    // require value, set to SettingV(),
    // it will force custom to set in config
    
    Layer<xpu>::Require();
  }
  
  virtual void SetupLayer(std::map<std::string, SettingV> &setting,
                          const std::vector<Node<xpu>*> &bottom,
                          const std::vector<Node<xpu>*> &top,
                          mshadow::Random<xpu> *prnd) {
    Layer<xpu>::SetupLayer(setting, bottom, top, prnd);

    b = setting["b"].fVal();
    utils::Check(b >= 0.0f, "ELU Layer: parameter b must non-negative.");
  }
  
  virtual void Reshape(const std::vector<Node<xpu>*> &bottom,
                       const std::vector<Node<xpu>*> &top,
                       bool show_info = false) {
    utils::Check(bottom.size() == BottomNodeNum(),
                  "ELULayer:bottom size problem."); 
    utils::Check(top.size() == TopNodeNum(),
                  "ELULayer:top size problem.");
    top[0]->Resize(bottom[0]->data.shape_, bottom[0]->length.shape_);

    if (show_info) {
      bottom[0]->PrintShape("bottom0");
      top[0]->PrintShape("top0");
    }
  }

  virtual void CheckReshape(const std::vector<Node<xpu>*> &bottom,
                            const std::vector<Node<xpu>*> &top) {
    // Check for reshape
    bool need_reshape = false;
    if (! (bottom[0]->data.shape_ == top[0]->data.shape_)) {
        need_reshape = true;
    }

    // Do reshape 
    if (need_reshape) {
        this->Reshape(bottom, top);
    }
  }
  
  virtual void Forward(const std::vector<Node<xpu>*> &bottom,
                       const std::vector<Node<xpu>*> &top) {
    using namespace mshadow::expr;
    top[0]->length = F<op::identity>(bottom[0]->length);
    top[0]->data = F<op::elu>(bottom[0]->data, b);
  }
  
  virtual void Backprop(const std::vector<Node<xpu>*> &bottom,
                        const std::vector<Node<xpu>*> &top) {
    using namespace mshadow::expr;
    if (this->prop_error[0]) {
      bottom[0]->diff += F<op::elu_grad>(top[0]->data, b) * top[0]->diff;
    }
  }

 protected:
  float b;

};
}  // namespace layer
}  // namespace textnet
#endif  // LAYER_ELU_LAYER_INL_HPP_

