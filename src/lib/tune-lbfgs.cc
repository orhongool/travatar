#include <cfloat>
#include <map>
#include <liblbfgs/lbfgs.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#include <travatar/tune-lbfgs.h>
#include <travatar/gradient.h>
#include <travatar/global-debug.h>
#include <travatar/softmax.h>
#include <travatar/dict.h>
#include <travatar/weights.h>
#include <travatar/eval-measure.h>
#include <travatar/eval-measure-bleu.h>
#include <travatar/tuning-example.h>

using namespace std;
using namespace boost;
using namespace travatar;

TuneLbfgs::~TuneLbfgs() {
    if(gradient_)
        delete gradient_;
}

void TuneLbfgs::Init(const SparseMap & init_weights) {
    gradient_->Init(init_weights, examps_);
}

Real TuneLbfgs::operator()(size_t n, const Real * x, Real * g) const {
    return gradient_->CalcGradient(n, x, g);
}

// Tune new weights using the expected BLEU algorithm
Real TuneLbfgs::RunTuning(SparseMap & kv) {

    // Sanity checks
    if(examps_.size() < 1)
        THROW_ERROR("Must have at least one example to perform tuning");
    if(l1_coeff_ != 0.0)
        THROW_ERROR("L1 regularization is not supported yet.");

    // Start
    PRINT_DEBUG("Starting L-BFGS Tuning Run: " << Dict::PrintSparseMap(kv) << endl, 2);

    // The final score
    Real last_score = 0.0;

    // Initialize the gradient
    gradient_->SetMult(-1);

    // Initialize the weights appropriately
    vector<Real> weights;
    if(!use_init_)
        kv = SparseMap();
    gradient_->DensifyWeights(kv, weights);

    // Optimize and save
    liblbfgs::LBFGS<TuneLbfgs> lbfgs(*this, iters_, l1_coeff_, 1);
    last_score = lbfgs(weights.size(), &(*weights.begin()));
    
    gradient_->SparsifyWeights(weights, kv);

    return last_score * -1;
}
