// Copyright 2002-2021 CEA LIST
// SPDX-FileCopyrightText: 2022 CEA LIST <gael.de-chalendar@cea.fr>
//
// SPDX-License-Identifier: MIT

#ifndef DEEPLIMA_LEMMATIZATION_IMPL_H
#define DEEPLIMA_LEMMATIZATION_IMPL_H

#include "deeplima/token_type.h"
#include "deeplima/nets/birnn_seq2seq.h"
#include "deeplima/eigen_wrp/embd_dict.h"
#include "deeplima/eigen_wrp/dict_embd_vectorizer.h"
#include "deeplima/utils/str_index.h"
#include "morph_model/morph_model.h"
#include "helpers/path_resolver.h"

namespace deeplima
{
namespace lemmatization
{

namespace impl {

using EmbdVectorizer = DictEmbdVectorizer<EmbdUInt64FloatHolder, EmbdUInt64Float, eigen_wrp::EigenMatrixXf>;

class LemmatizationImpl: public RnnSeq2Seq
{
public:

  LemmatizationImpl();

  LemmatizationImpl(
      size_t threads,
      size_t buffer_size_per_thread
    );

  virtual ~LemmatizationImpl() = default;

  virtual void load(const std::string& fn, const PathResolver& /*path_resolver*/);

  void init(size_t max_input_word_len,
            const std::vector<std::string>& class_names,
            const std::vector<std::vector<std::string>>& class_values);

  /** @return true if the upos at index @param idx is fixed in the features matrix @param classes.
   * The upos line in the matrix is given by the member @ref m_upos_idx.
   */
  bool is_fixed(std::shared_ptr< StdMatrix<uint8_t> > classes, size_t idx);

  morph_model::morph_feats_t get_morph_feats(std::shared_ptr< StdMatrix<uint8_t> > classes, size_t idx) const;

  void predict(const std::u32string& form,
               std::shared_ptr< StdMatrix<uint8_t> > classes, size_t idx,
               std::u32string& target);


protected:
  size_t m_beam_size;
  EmbdVectorizer m_vectorizer;
  EmbdVectorizer m_feat_vectorizer;
  std::vector<size_t> m_feat2cls;
  /** index in the features matrix of the upos line*/
  size_t m_upos_idx;
  /** index i is true if upos whose index is i is considered as fixed*/
  std::vector<bool> m_fixed_upos;
};

} // namespace impl
} // namespace lemmatization
} // namespace deeplima

#endif // DEEPLIMA_LEMMATIZATION_IMPL_H

