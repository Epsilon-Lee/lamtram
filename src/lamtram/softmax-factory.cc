
#include <lamtram/softmax-factory.h>
#include <lamtram/softmax-full.h>
#include <lamtram/softmax-class.h>
#include <lamtram/softmax-mod.h>
#include <lamtram/sentence.h>
#include <lamtram/macros.h>
#include <fstream>


using namespace std;
using namespace lamtram;

SoftmaxPtr SoftmaxFactory::CreateSoftmax(const std::string & sig, int input_size, const VocabularyPtr & vocab, cnn::Model & mod) {
  if(sig == "full") {
    return SoftmaxPtr(new SoftmaxFull(sig, input_size, vocab, mod));
  } else if(sig.substr(0,5) == "class") {
    return SoftmaxPtr(new SoftmaxClass(sig, input_size, vocab, mod));
  } else if(sig.substr(0,3) == "mod") {
    return SoftmaxPtr(new SoftmaxMod(sig, input_size, vocab, mod));
  } else {
    THROW_ERROR("Bad softmax signature" << sig);
  }
}
