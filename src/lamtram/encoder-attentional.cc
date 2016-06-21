#include <lamtram/encoder-attentional.h>
#include <lamtram/macros.h>
#include <lamtram/builder-factory.h>
#include <cnn/model.h>
#include <cnn/nodes.h>
#include <cnn/rnn.h>
#include <boost/range/irange.hpp>
#include <ctime>
#include <fstream>

using namespace std;
using namespace lamtram;


ExternAttentional::ExternAttentional(const std::vector<LinearEncoderPtr> & encoders,
                   const std::string & attention_type, int state_size,
                   cnn::Model & mod)
    : ExternCalculator(0), encoders_(encoders),
      attention_type_(attention_type), hidden_size_(0), state_size_(state_size) {
  for(auto & enc : encoders)
    context_size_ += enc->GetNumNodes();

  if(attention_type == "dot") {
    // No parameters for dot product
  } else if(attention_type_ == "bilin") {
    p_ehid_h_W_ = mod.add_parameters({(unsigned int)state_size_, (unsigned int)context_size_});
  } else if(attention_type_.substr(0,4) == "mlp:") {
    hidden_size_ = stoi(attention_type_.substr(4));
    assert(hidden_size_ != 0);
    p_ehid_h_W_ = mod.add_parameters({(unsigned int)hidden_size_, (unsigned int)context_size_});
    p_ehid_state_W_ = mod.add_parameters({(unsigned int)hidden_size_, (unsigned int)state_size_});
    p_e_ehid_W_ = mod.add_parameters({1, (unsigned int)hidden_size_});
  } else {
    THROW_ERROR("Illegal attention type");
  }
}


// Index the parameters in a computation graph
void ExternAttentional::NewGraph(cnn::ComputationGraph & cg) {
  for(auto & enc : encoders_)
    enc->NewGraph(cg);
  if(attention_type_ != "dot")
    i_ehid_h_W_ = parameter(cg, p_ehid_h_W_);
  if(hidden_size_) {
    i_ehid_state_W_ = parameter(cg, p_ehid_state_W_);
    i_e_ehid_W_ = parameter(cg, p_e_ehid_W_);
  }
  curr_graph_ = &cg;

}

ExternAttentional* ExternAttentional::Read(std::istream & in, cnn::Model & model) {
  int num_encoders, state_size;
  string version_id, attention_type, line;
  if(!getline(in, line))
    THROW_ERROR("Premature end of model file when expecting ExternAttentional");
  istringstream iss(line);
  iss >> version_id >> num_encoders >> attention_type >> state_size;
  if(version_id != "extatt_002")
    THROW_ERROR("Expecting a ExternAttentional of version extatt_002, but got something different:" << endl << line);
  vector<LinearEncoderPtr> encoders;
  while(num_encoders-- > 0)
    encoders.push_back(LinearEncoderPtr(LinearEncoder::Read(in, model)));
  return new ExternAttentional(encoders, attention_type, state_size, model);
}
void ExternAttentional::Write(std::ostream & out) {
  out << "extatt_002 " << encoders_.size() << " " << attention_type_ << " " << state_size_ << endl;
  for(auto & enc : encoders_) enc->Write(out);
}


void ExternAttentional::InitializeSentence(
      const Sentence & sent_src, bool train, cnn::ComputationGraph & cg) {

  // First get the states in a digestable format
  vector<vector<cnn::expr::Expression> > hs_sep;
  for(auto & enc : encoders_) {
    enc->BuildSentGraph(sent_src, true, train, cg);
    hs_sep.push_back(enc->GetWordStates());
    assert(hs_sep[0].size() == hs_sep.rbegin()->size());
  }
  sent_len_ = hs_sep[0].size();
  // Concatenate them if necessary
  vector<cnn::expr::Expression> hs_comb;
  if(encoders_.size() == 1) {
    hs_comb = hs_sep[0];
  } else {
    for(int i : boost::irange(0, sent_len_)) {
      vector<cnn::expr::Expression> vars;
      for(int j : boost::irange(0, (int)encoders_.size()))
        vars.push_back(hs_sep[j][i]);
      hs_comb.push_back(concatenate(vars));
    }
  }
  if(hs_comb.size() >= 512) {
    cg.PrintGraphviz();
    THROW_ERROR("Oversized sentence combination (size="<<hs_comb.size()<<"): " << sent_src);
  }
  i_h_ = concatenate_cols(hs_comb);
  i_h_last_ = *hs_comb.rbegin();

  // Create an identity with shape
  if(hidden_size_) {
    i_ehid_hpart_ = i_ehid_h_W_*i_h_;
    sent_values_.resize(sent_len_, 1.0);
    i_sent_len_ = input(cg, {1, (unsigned int)sent_len_}, &sent_values_);
  } else if(attention_type_ == "dot") {
    i_ehid_hpart_ = transpose(i_h_);
  } else if(attention_type_ == "bilin") {
    i_ehid_hpart_ = transpose(i_ehid_h_W_*i_h_);
  } else {
    THROW_ERROR("Bad attention type " << attention_type_);
  }

}

void ExternAttentional::InitializeSentence(
      const std::vector<Sentence> & sent_src, bool train, cnn::ComputationGraph & cg) {

  // First get the states in a digestable format
  vector<vector<cnn::expr::Expression> > hs_sep;
  for(auto & enc : encoders_) {
    enc->BuildSentGraph(sent_src, true, train, cg);
    hs_sep.push_back(enc->GetWordStates());
    assert(hs_sep[0].size() == hs_sep.rbegin()->size());
  }
  sent_len_ = hs_sep[0].size();
  // Concatenate them if necessary
  vector<cnn::expr::Expression> hs_comb;
  if(encoders_.size() == 1) {
    hs_comb = hs_sep[0];
  } else {
    for(int i : boost::irange(0, sent_len_)) {
      vector<cnn::expr::Expression> vars;
      for(int j : boost::irange(0, (int)encoders_.size()))
        vars.push_back(hs_sep[j][i]);
      hs_comb.push_back(concatenate(vars));
    }
  }
  if(hs_comb.size() >= 512) {
    cg.PrintGraphviz();
    THROW_ERROR("Oversized sentence combination (size="<<hs_comb.size()<<"): " << sent_src);
  }
  i_h_ = concatenate_cols(hs_comb);
  i_h_last_ = *hs_comb.rbegin();

  // Create an identity with shape
  if(hidden_size_) {
    i_ehid_hpart_ = i_ehid_h_W_*i_h_;
    sent_values_.resize(sent_len_, 1.0);
    i_sent_len_ = input(cg, {1, (unsigned int)sent_len_}, &sent_values_);
  } else if(attention_type_ == "dot") {
    i_ehid_hpart_ = transpose(i_h_);
  } else if(attention_type_ == "bilin") {
    i_ehid_hpart_ = transpose(i_ehid_h_W_*i_h_);
  } else {
    THROW_ERROR("Bad attention type " << attention_type_);
  }

}

cnn::expr::Expression ExternAttentional::GetEmptyContext(cnn::ComputationGraph & cg) const {
  return zeroes(cg, {(unsigned int)state_size_});
}

// Create a variable encoding the context
cnn::expr::Expression ExternAttentional::CreateContext(
    const std::vector<cnn::expr::Expression> & state_in,
    bool train,
    cnn::ComputationGraph & cg,
    std::vector<cnn::expr::Expression> & align_out) const {
  if(&cg != curr_graph_)
    THROW_ERROR("Initialized computation graph and passed comptuation graph don't match."); 
  cnn::expr::Expression i_ehid, i_e;
  // MLP
  if(hidden_size_) {
    if(state_in.size()) {
      // i_ehid_state_W_ is {hidden_size, state_size}, state_in is {state_size, 1}
      cnn::expr::Expression i_ehid_spart = i_ehid_state_W_ * *state_in.rbegin();
      i_ehid = affine_transform({i_ehid_hpart_, i_ehid_spart, i_sent_len_});
    } else {
      i_ehid = i_ehid_hpart_;
    }
    // Run through nonlinearity
    cnn::expr::Expression i_ehid_out = tanh({i_ehid});
    // i_e_ehid_W_ is {1, hidden_size}, i_ehid_out is {hidden_size, sent_len}
    i_e = transpose(i_e_ehid_W_ * i_ehid_out);
  // Bilinear/dot product
  } else {
    assert(state_in.size() > 0);
    i_e = i_ehid_hpart_ * (*state_in.rbegin());
  }
  cnn::expr::Expression i_alpha = softmax({i_e});
  align_out.push_back(i_alpha);
  // Print alignments
  if(GlobalVars::verbose >= 2) {
    vector<cnn::real> softmax = as_vector(cg.incremental_forward());
    cerr << "Alignments: " << softmax << endl;
  }
  // i_h_ is {input_size, sent_len}, i_alpha is {sent_len, 1}
  return i_h_ * i_alpha; 
}

EncoderAttentional::EncoderAttentional(
           const ExternAttentionalPtr & extern_calc,
           const NeuralLMPtr & decoder,
           cnn::Model & model)
  : extern_calc_(extern_calc), decoder_(decoder), curr_graph_(NULL) {
  // Encoder to decoder mapping parameters
  int enc2dec_in = extern_calc->GetContextSize();
  int enc2dec_out = decoder_->GetNumLayers() * decoder_->GetNumNodes();
  p_enc2dec_W_ = model.add_parameters({(unsigned int)enc2dec_out, (unsigned int)enc2dec_in});
  p_enc2dec_b_ = model.add_parameters({(unsigned int)enc2dec_out});
}


void EncoderAttentional::NewGraph(cnn::ComputationGraph & cg) {
  extern_calc_->NewGraph(cg);
  decoder_->NewGraph(cg);
  i_enc2dec_b_ = parameter(cg, p_enc2dec_b_);
  i_enc2dec_W_ = parameter(cg, p_enc2dec_W_);
  curr_graph_ = &cg;
}

template <class SentData>
std::vector<cnn::expr::Expression> EncoderAttentional::GetEncodedState(const SentData & sent_src, bool train, cnn::ComputationGraph & cg) {
  extern_calc_->InitializeSentence(sent_src, train, cg);
  cnn::expr::Expression i_decin = affine_transform({i_enc2dec_b_, i_enc2dec_W_, extern_calc_->GetState()});
  // Perform transformation
  vector<cnn::expr::Expression> decoder_in(decoder_->GetNumLayers() * 2);
  for (int i = 0; i < decoder_->GetNumLayers(); ++i) {
    decoder_in[i] = (decoder_->GetNumLayers() == 1 ?
                     i_decin :
                     pickrange({i_decin}, i * decoder_->GetNumNodes(), (i + 1) * decoder_->GetNumNodes()));
    decoder_in[i + decoder_->GetNumLayers()] = tanh({decoder_in[i]});
  }
  return decoder_in;
}

template
std::vector<cnn::expr::Expression> EncoderAttentional::GetEncodedState<Sentence>(const Sentence & sent_src, bool train, cnn::ComputationGraph & cg);
template
std::vector<cnn::expr::Expression> EncoderAttentional::GetEncodedState<std::vector<Sentence> >(const std::vector<Sentence> & sent_src, bool train, cnn::ComputationGraph & cg);

template <class SentData>
cnn::expr::Expression EncoderAttentional::BuildSentGraph(
          const SentData & sent_src, const SentData & sent_trg,
          const SentData & sent_cache,
          float samp_percent,
          bool train,
          cnn::ComputationGraph & cg, LLStats & ll) {
  if(&cg != curr_graph_)
    THROW_ERROR("Initialized computation graph and passed comptuation graph don't match."); 
  std::vector<cnn::expr::Expression> decoder_in = GetEncodedState(sent_src, train, cg);
  return decoder_->BuildSentGraph(sent_trg, sent_cache, extern_calc_.get(), decoder_in, samp_percent, train, cg, ll);
}

cnn::expr::Expression EncoderAttentional::SampleTrgSentences(const Sentence & sent_src,
                                                             const Sentence * sent_trg,
                                                             int num_samples,
                                                             int max_len,
                                                             bool train,
                                                             cnn::ComputationGraph & cg,
                                                             vector<Sentence> & samples) {
  if(&cg != curr_graph_)
    THROW_ERROR("Initialized computation graph and passed comptuation graph don't match."); 
  std::vector<cnn::expr::Expression> decoder_in = GetEncodedState(sent_src, train, cg);
  return decoder_->SampleTrgSentences(extern_calc_.get(), decoder_in, sent_trg, num_samples, max_len, train, cg, samples);
}

template
cnn::expr::Expression EncoderAttentional::BuildSentGraph<Sentence>(
  const Sentence & sent_src, const Sentence & sent_trg, const Sentence & sent_cache,
  float samp_percent,
  bool train, cnn::ComputationGraph & cg, LLStats & ll);
template
cnn::expr::Expression EncoderAttentional::BuildSentGraph<vector<Sentence> >(
  const vector<Sentence> & sent_src, const vector<Sentence> & sent_trg, const vector<Sentence> & sent_cache,
  float samp_percent,
  bool train, cnn::ComputationGraph & cg, LLStats & ll);


EncoderAttentional* EncoderAttentional::Read(const DictPtr & vocab_src, const DictPtr & vocab_trg, std::istream & in, cnn::Model & model) {
  string version_id, line;
  if(!getline(in, line))
    THROW_ERROR("Premature end of model file when expecting EncoderAttentional");
  istringstream iss(line);
  iss >> version_id;
  if(version_id != "encatt_001")
    THROW_ERROR("Expecting a EncoderAttentional of version encatt_001, but got something different:" << endl << line);
  ExternAttentionalPtr extern_calc(ExternAttentional::Read(in, model));
  NeuralLMPtr decoder(NeuralLM::Read(vocab_trg, in, model));
  return new EncoderAttentional(extern_calc, decoder, model);
}
void EncoderAttentional::Write(std::ostream & out) {
  out << "encatt_001" << endl;
  extern_calc_->Write(out);
  decoder_->Write(out);
}

