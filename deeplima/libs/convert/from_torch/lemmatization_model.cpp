// Copyright 2002-2021 CEA LIST
// SPDX-FileCopyrightText: 2022 CEA LIST <gael.de-chalendar@cea.fr>
//
// SPDX-License-Identifier: MIT

#include "deeplima/eigen_wrp/lemmatization_eigen_inference_impl.h"
#include "tasks/lemmatization/model/seq2seq_for_lemmatization.h"

#include "convert_from_torch.h"


#include "deeplima/utils/split_string.h"

using namespace std;
using namespace torch;
using namespace deeplima::convert_from_torch;
using namespace deeplima::eigen_impl;

namespace deeplima
{
namespace lemmatization
{
namespace eigen_impl
{

void BiRnnSeq2SeqEigenInferenceForLemmatization::convert_from_torch(const std::string& fn)
{
  // std::cerr << "BiRnnSeq2SeqEigenInferenceForLemmatization::convert_from_torch " << fn << std::endl;
  train::Seq2SeqLemmatizerImpl src;
  try
  {
    torch::load(src, fn, torch::Device(torch::kCPU));
  }
  catch (const c10::Error& e)
  {
    std::cerr << "Exception while trying to load Torch model file " << fn << std::endl
              << e.what_without_backtrace();
    throw std::runtime_error(e.what_without_backtrace());
  }

  // dicts and embeddings
  Parent::convert_dicts_and_embeddings(src);

  m_morph_model = src.get_morph_model();
  // upos found as fixed during training are retrieved here as string
  vector<string> fixed_upos = utils::split(src.get_fixed_upos(), ' ');
  m_fixed_upos.reserve(fixed_upos.size());
  // store int representation of fixed upos
  for (const string& s : fixed_upos)
  {
    m_fixed_upos.push_back(m_morph_model.get_upos_id(s));
  }

  // torch modules
  Parent::m_lstm.reserve(src.get_layers_lstm().size());
  for (size_t i = 0; i < src.get_layers_lstm().size(); i++)
  {
    const std::string name = src.get_module_name(i, "lstm");
    Parent::m_lstm_idx[name] = i;

    const nn::LSTM& m = src.get_layers_lstm()[i];
    Parent::m_lstm.emplace_back(typename Parent::params_bilstm_spec_t());
    typename Parent::params_bilstm_spec_t& layer = Parent::m_lstm.back();

    convert_module_from_torch(m, layer);
  }

  Parent::m_multi_bilstm.emplace_back(std::make_shared<typename Parent::params_multilayer_bilstm_spec_t>(Parent::m_lstm));

  Parent::m_linear.reserve(src.get_layers_linear().size());
  for (size_t i = 0; i < src.get_layers_linear().size(); i++)
  {
    const std::string name = src.get_module_name(i, "linear");
    Parent::m_linear_idx[name] = i;

    const nn::Linear& m = src.get_layers_linear()[i];
    Parent::m_linear.emplace_back(params_linear_t<Eigen::MatrixXf, Eigen::VectorXf>());
    params_linear_t<Eigen::MatrixXf, Eigen::VectorXf>& layer = Parent::m_linear.back();

    convert_module_from_torch(m, layer);
  }

  // temp: create exec plan
  // Encoder
  Parent::m_ops.push_back(std::make_shared<Op_BiLSTM<Eigen::MatrixXf, Eigen::VectorXf, float>>());
  // Parent::m_params.push_back(std::make_shared<params_multilayer_bilstm_t<Eigen::MatrixXf, Eigen::VectorXf>>());
  // auto p_enc = std::dynamic_pointer_cast<params_multilayer_bilstm_t<Eigen::MatrixXf, Eigen::VectorXf>>(Parent::m_params.back());
  // *p_enc = Parent::m_multi_bilstm[Parent::m_lstm_idx["encoder_lstm_0"]];
  Parent::m_params.push_back(Parent::m_multi_bilstm[Parent::m_lstm_idx["encoder_lstm_0"]]);


  // Linear for decoder init state H
  Parent::m_ops.push_back(std::make_shared<Op_Linear<Eigen::MatrixXf, Eigen::VectorXf, float>>());
  Parent::m_params.push_back(std::make_shared<params_linear_t<Eigen::MatrixXf, Eigen::VectorXf>>());
  auto p_fc_h0 = std::dynamic_pointer_cast<params_linear_t<Eigen::MatrixXf, Eigen::VectorXf>>(Parent::m_params.back());
  *p_fc_h0 = Parent::m_linear[Parent::m_linear_idx["interm_fc_h0"]];

  // Linear for decoder init state C
  Parent::m_ops.push_back(std::make_shared<Op_Linear<Eigen::MatrixXf, Eigen::VectorXf, float>>());
  Parent::m_params.push_back(std::make_shared<params_linear_t<Eigen::MatrixXf, Eigen::VectorXf>>());
  auto p_fc_c0 = std::dynamic_pointer_cast<params_linear_t<Eigen::MatrixXf, Eigen::VectorXf>>(Parent::m_params.back());
  *p_fc_c0 = Parent::m_linear[Parent::m_linear_idx["interm_fc_c0"]];

  // Linear for features encoder
  // input to decoder
  Parent::m_ops.push_back(std::make_shared<Op_Linear<Eigen::MatrixXf, Eigen::VectorXf, float>>());
  Parent::m_params.push_back(std::make_shared<params_linear_t<Eigen::MatrixXf, Eigen::VectorXf>>());
  auto p_fc_feats_dec = std::dynamic_pointer_cast<params_linear_t<Eigen::MatrixXf, Eigen::VectorXf>>(Parent::m_params.back());
  *p_fc_feats_dec = Parent::m_linear[Parent::m_linear_idx["fc_cat2decoder"]];

  // Decoder
  Parent::m_ops.push_back(std::make_shared<Op_LSTM_Beam_Decoder<Eigen::MatrixXf, Eigen::VectorXf, float>>());
  Parent::m_params.push_back(std::make_shared<params_lstm_beam_decoder_t<Eigen::MatrixXf, Eigen::VectorXf>>());
  auto p_dec = std::dynamic_pointer_cast<params_lstm_beam_decoder_t<Eigen::MatrixXf, Eigen::VectorXf>>(Parent::m_params.back());
  p_dec->lstm = Parent::m_lstm[Parent::m_lstm_idx["decoder_lstm_0"]].fw;
  p_dec->linear = Parent::m_linear[Parent::m_linear_idx["fc_output"]];

  // Precompute all decoder's embeddings
  Eigen::MatrixXf precomputed_embd_tensor = Eigen::MatrixXf::Zero(p_dec->lstm.weight_ih.rows(),
                                      Parent::m_input_uint_dicts[1].get_tensor().cols());
  std::shared_ptr<Op_LSTM_Beam_Decoder<Eigen::MatrixXf, Eigen::VectorXf, float>> decoder
    = std::dynamic_pointer_cast<Op_LSTM_Beam_Decoder<Eigen::MatrixXf, Eigen::VectorXf, float>>(Parent::m_ops.back());
  decoder->precompute_inputs(p_dec, Parent::m_input_uint_dicts[1].get_tensor(), precomputed_embd_tensor, 0);
  Parent::m_input_uint_dicts[1].set_tensor(precomputed_embd_tensor);

  // input to encoder
  Parent::m_ops.push_back(std::make_shared<Op_Linear<Eigen::MatrixXf, Eigen::VectorXf, float>>());
  Parent::m_params.push_back(std::make_shared<params_linear_t<Eigen::MatrixXf, Eigen::VectorXf>>());
  auto p_fc_feats_enc = std::dynamic_pointer_cast<params_linear_t<Eigen::MatrixXf, Eigen::VectorXf>>(Parent::m_params.back());
  *p_fc_feats_enc = Parent::m_linear[Parent::m_linear_idx["fc_cat2encoder"]];

  Parent::m_wb.resize(6);

  // tags
  // cerr << "TAGS:" << endl;
  // for ( const auto& it : src.get_tags() )
  // {
  //   cerr << "\t" << it.first << " = " << it.second << endl;
  // }
  // cerr << endl;
}

} // namespace eigen_impl
} // namespace tagging
} // namespace deeplima

