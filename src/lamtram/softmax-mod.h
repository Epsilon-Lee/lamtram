#pragma once

#include <cnn/expr.h>
#include <lamtram/softmax-base.h>
#include <lamtram/dist-base.h>

namespace cnn { class Parameters; }

namespace lamtram {

// An interface to a class that takes a vector as input
// (potentially batched) and calculates a probability distribution
// over words
class SoftmaxMod : public SoftmaxBase {

public:
  SoftmaxMod(const std::string & sig, int input_size, const DictPtr & vocab, cnn::Model & mod);
  ~SoftmaxMod() { };

  // A pair of a context and distribution values
  typedef std::pair<std::vector<float>, std::vector<float> > CtxtDist;

  // Create a new graph
  virtual void NewGraph(cnn::ComputationGraph & cg) override;

  // Calculate training loss for one word
  virtual cnn::expr::Expression CalcLoss(cnn::expr::Expression & in, const Sentence & ngram, bool train) override;
  // Calculate training loss for multiple words
  virtual cnn::expr::Expression CalcLoss(cnn::expr::Expression & in, const std::vector<Sentence> & ngrams, bool train) override;

  // Calculate loss using cached info
  virtual cnn::expr::Expression CalcLossCache(cnn::expr::Expression & in, const Sentence & ngram, std::pair<int,int> sent_word, bool train) override;
  virtual cnn::expr::Expression CalcLossCache(cnn::expr::Expression & in, const std::vector<Sentence> & ngrams, const std::vector<std::pair<int,int> > & sent_words, bool train) override;
  
  // Calculate the full probability distribution
  virtual cnn::expr::Expression CalcProbability(cnn::expr::Expression & in) override;
  virtual cnn::expr::Expression CalcLogProbability(cnn::expr::Expression & in) override;

  virtual void Cache(const std::vector<Sentence> sents, const std::vector<int> set_ids) override;

protected:

  cnn::expr::Expression CalcLossExpr(cnn::expr::Expression & in, const CtxtDist & ctxt_dist, WordId wid, bool train);

  void LoadDists(int id);

  void CalcDists(const Sentence & ngram, CtxtDist & ctxt_dist);

  int num_dist_, num_ctxt_;

  cnn::Parameters *p_sms_W_, *p_smd_W_; // Softmax weights
  cnn::Parameters *p_sms_b_, *p_smd_b_; // Softmax bias

  cnn::expr::Expression i_sms_W_, i_smd_W_;
  cnn::expr::Expression i_sms_b_, i_smd_b_;

  float dropout_; // How much to drop out the dense distributions (at training)
  std::vector<DistPtr> dist_ptrs_;
  int dist_id_;

  std::vector<CtxtDist> cache_;
  std::vector<std::vector<int> > cache_ids_;
  std::vector<std::string> wildcards_;
  std::vector<std::vector<std::string> > dist_files_;

};

}
