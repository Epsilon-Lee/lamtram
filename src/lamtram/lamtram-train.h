#pragma once

#include <lamtram/vocabulary.h>
#include <lamtram/sentence.h>
#include <cnn/tensor.h>
#include <boost/program_options.hpp>
#include <string>

namespace cnn {
struct Trainer;
class Model;
}

namespace lamtram {

class Vocabulary;

class CnntransTrain {

public:
    CnntransTrain() { }
    int main(int argc, char** argv);
    
    void TrainLM();
    void TrainEncDec();
    void TrainEncAtt();
    void TrainEncCls();

    template<class ModelType, class OutputType>
    void BilingualTraining(const std::vector<Sentence> & train_src,
                           const std::vector<OutputType> & train_trg,
                           const std::vector<Sentence> & dev_src,
                           const std::vector<OutputType> & dev_trg,
                           const Vocabulary & vocab_src,
                           const Vocabulary & vocab_trg,
                           cnn::Model & mod,
                           ModelType & encdec);

    // Get the trainer to use
    typedef std::shared_ptr<cnn::Trainer> TrainerPtr;
    TrainerPtr GetTrainer(const std::string & trainer_id, const cnn::real learning_rate, cnn::Model & model);

    // Load in the training data
    void LoadFile(const std::string filename, int pad, bool add_last, Vocabulary & vocab, std::vector<Sentence> & sents);
    void LoadLabels(const std::string filename, Vocabulary & vocab, std::vector<int> & labs);

protected:

    boost::program_options::variables_map vm_;

    // Variable settings
    cnn::real rate_thresh_;
    int epochs_, context_;
    std::string model_in_file_, model_out_file_;
    std::string train_file_trg_, dev_file_trg_, train_file_src_, dev_file_src_;

};

}
