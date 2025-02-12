// Copyright 2021 CEA LIST
// SPDX-FileCopyrightText: 2022 CEA LIST <gael.de-chalendar@cea.fr>
//
// SPDX-License-Identifier: MIT

#ifndef DEEPLIMA_NER_H
#define DEEPLIMA_NER_H

#include "config.h"

#include "nets/birnn_seq_cls.h"

#if DEEPLIMA_INFERENCE_ENGINE == IE_TORCH

//#include "torch_wrp/dict_vectorizer.h"
//#include "static_graph/dict.h"

#endif

#if DEEPLIMA_INFERENCE_ENGINE == IE_EIGEN

#include "eigen_wrp/eigen_matrix.h"
#include "eigen_wrp/embd_dict.h"
#include "eigen_wrp/dict_embd_vectorizer.h"
#include "eigen_wrp/tagging_eigen_inference_impl.h"

#endif

#include "tagging/impl/tagging_wrapper.h"
#include "eigen_wrp/word_seq_embd_vectorizer.h"
#include "feat_extractors.h"

namespace deeplima
{
namespace tagging
{

#if DEEPLIMA_INFERENCE_ENGINE == IE_TORCH

namespace torch_impl
{
  //typedef impl::SegmentationInferenceWrapper<train::BiRnnClassifierForSegmentationImpl> Model;
  //typedef DictVectorizer<DictsHolder, UInt64Dict, TorchMatrix<int64_t>> EmbdVectorizer;

} // namespace torch_impl

#elif DEEPLIMA_INFERENCE_ENGINE == IE_EIGEN

namespace eigen_impl
{
  template <typename AuxScalar=float>
  class Model : public impl::TaggingInferenceWrapper<BiRnnEigenInferenceForTagging<AuxScalar>>
  {};

  typedef DictEmbdVectorizer<EmbdUInt64FloatHolder, EmbdUInt64Float, eigen_wrp::EigenMatrixXf> EmbdVectorizer;
  /*typedef WordSeqEmbdVectorizer<token_buffer_t,
                                TokenStrFeatExtractor<token_buffer_t::token_t>,
                                TokenUIntFeatExtractor<token_buffer_t::token_t>,
                                eigen_wrp::EigenMatrixXf> FeaturesVectorizer;*/

  template <class TokenVector, class Token>
  class FeaturesVectorizer:
      public vectorizers::WordSeqEmbdVectorizer<TokenVector,
                                                TokenStrFeatExtractor<Token>,
                                                TokenUIntFeatExtractor<Token>,
                                                eigen_wrp::EigenMatrixXf::matrix_t,
                                                Eigen::Index>
  {};

  template <class TokenVector, class Token>
  class FeaturesVectorizerWithCache:
      public vectorizers::WordSeqEmbdVectorizerWithCache<TokenVector,
                                                         TokenStrFeatExtractor<Token>,
                                                         TokenUIntFeatExtractor<Token>,
                                                         eigen_wrp::EigenMatrixXf::matrix_t,
                                                         Eigen::Index>
  {};

  template <class Model, class TokenVector, class Token>
  class FeaturesVectorizerWithPrecomputing:
      public vectorizers::WordSeqEmbdVectorizerWithPrecomputing<Model,
                                                                TokenVector,
                                                                TokenStrFeatExtractor<Token>,
                                                                TokenUIntFeatExtractor<Token>,
                                                                eigen_wrp::EigenMatrixXf::matrix_t,
                                                                Eigen::Index>
  {};

} // namespace eigen_impl

#else
#error Unknown inference engine
#endif

namespace impl
{
#if DEEPLIMA_INFERENCE_ENGINE == IE_TORCH

  //typedef torch_impl::Model Model;
  //typedef torch_impl::EmbdVectorizer EmbdVectorizer;

#elif DEEPLIMA_INFERENCE_ENGINE == IE_EIGEN

  template <typename AuxScalar=float>
  class Model : public eigen_impl::Model<AuxScalar>
  {};

  template <class TokenVector, class Token>
  class FeaturesVectorizer: public eigen_impl::FeaturesVectorizer<TokenVector, Token> {};

  template <class TokenVector, class Token>
  class FeaturesVectorizerWithCache: public eigen_impl::FeaturesVectorizerWithCache<TokenVector, Token> {};

  template <class Model, class TokenVector, class Token>
  class FeaturesVectorizerWithPrecomputing: public eigen_impl::FeaturesVectorizerWithPrecomputing<Model, TokenVector, Token> {};

  typedef eigen_wrp::EigenMatrixXf BaseMatrix;

#else
#error Unknown inference engine
#endif

  /**
   * A kind of RnnSequenceClassifier, used for named entities tagging (?), but
   * also the parent of TaggingImpl, used as member in TokenSequenceAnalyzer
   */
  template <typename AuxScalar=float>
  class EntityTaggingClassifier: public RnnSequenceClassifier<Model<AuxScalar>, BaseMatrix, uint8_t>
  {
  public:
    EntityTaggingClassifier() :
        RnnSequenceClassifier<Model<AuxScalar>, BaseMatrix, uint8_t>()
    {
    }

    // EntityTaggingClassifier(uint32_t max_feat,
    //                         uint32_t overlap,
    //                         uint32_t num_slots,
    //                         uint32_t slot_len,
    //                         uint32_t num_threads) :
    //     RnnSequenceClassifier<Model<AuxScalar>, BaseMatrix, uint8_t>(
    //       max_feat, overlap, num_slots, slot_len, num_threads)
    // {
    // }

    virtual ~EntityTaggingClassifier() = default;
  };

} // namespace impl

} // namespace tagging
} // namespace deeplima

#endif
