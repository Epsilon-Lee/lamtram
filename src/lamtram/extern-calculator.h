#pragma once

#include <lamtram/sentence.h>
#include <cnn/tensor.h>
#include <vector>
#include <memory>

namespace cnn {
struct ComputationGraph;
}

namespace lamtram {

class ExternCalculator {
public:

    ExternCalculator(int context_size) : context_size_(context_size) { }
    virtual ~ExternCalculator() { }

    virtual void InitializeSentence(const Sentence & sent, cnn::ComputationGraph & cg) { }

    // Create a variable encoding the context
    virtual cnn::expr::Expression CreateContext(
        const Sentence & sent, int loc,
        const std::vector<cnn::expr::Expression> & state_in,
        cnn::ComputationGraph & cg) const = 0;

    int GetSize() const { return context_size_; }

protected:
    int context_size_;

};

typedef std::shared_ptr<ExternCalculator> ExternCalculatorPtr;

}
