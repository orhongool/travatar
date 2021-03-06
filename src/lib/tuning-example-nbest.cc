#include <travatar/sparse-map.h>
#include <travatar/tuning-example-nbest.h>
#include <travatar/global-debug.h>
#include <travatar/weights.h>
#include <travatar/eval-measure.h>
#include <boost/foreach.hpp>
#include <cfloat>
#include <algorithm>

using namespace std;
using namespace travatar;
using namespace boost;

// Add weights
void TuningExampleNbest::CountWeights(set<WordId> & weights) {
    BOOST_FOREACH(const ExamplePair & examp, nbest_)
        BOOST_FOREACH(const SparsePair & val, examp.first.GetImpl())
            weights.insert(val.first);
}

// Calculate the potential gain for a single example given the current weights
SparseMap TuningExampleNbest::CalculatePotentialGain(const SparseMap & weights) {
    if(nbest_.size() == 0) return SparseMap();
    // Find the hypothesis to be chosen with the current weights
    int hyp = -1;
    Real hyp_score = -REAL_MAX;
    for(int i = 0; i < (int)nbest_.size(); i++) {
        Real my_score = weights * nbest_[i].first;
        if(my_score > hyp_score) {
            hyp = i;
            hyp_score = my_score;
        }
    }
    // Find all features that have the potential to cause a gain
    SparseMap ret;
    BOOST_FOREACH(const ExamplePair & examp, nbest_) {
        Real gain = examp.second->ConvertToScore() - nbest_[hyp].second->ConvertToScore();
        if(gain <= 0) continue; // Skip examples with no or negative gain
        SparseVector diff = examp.first - nbest_[hyp].first;
        BOOST_FOREACH(const SparseMap::value_type val, diff.GetImpl())
            if(val.second != 0) // Skip examples with same value as current ans
                ret[val.first] = max(ret[val.first], gain);
    }
    return ret;
}

inline Real FindIntersection(Real s1, Real v1, Real s2, Real v2) {
    return (s1==s2 ? REAL_MAX : (v1-v2)/(s2-s1));
}

// Get the convex hull, which consists of scored spans in order of the span location
ConvexHull TuningExampleNbest::CalculateConvexHull(
                        const SparseMap & weights,
                        const SparseMap & gradient) const {
    // First, get all the lines, these are tuples of
    // the slope of the line, the value at zero, and the score
    // These happen to be the same shape as ScoredSpans so we abuse notation
    vector<ScoredSpan> lines;
    BOOST_FOREACH(const ExamplePair & examp, nbest_) {
        Real slope = gradient * examp.first;
        Real val = weights * examp.first;
        lines.push_back(make_pair(make_pair(slope, val), examp.second));
    }
    // Sort in order of ascending slope
    sort(lines.begin(), lines.end());
    // Start at position zero
    // While we are not finished
    ConvexHull hull;
    Real prev_pos = -REAL_MAX;
    // In case of ties in slope, we want to start with the line with the highest value
    int i;
    for(i = 0; i+1 < (int)lines.size() && lines[i].first.first == lines[i+1].first.first; i++);
    // Cycle through the points
    while(i < (int)lines.size()) {
        // Find the line the crosses at the earliest point
        int best_j = lines.size();
        Real best_pos = REAL_MAX;
        for(int j = i+1; j < (int)lines.size(); j++) {
            // Find the intersection of the two lines
            Real my_pos =
                FindIntersection(
                    lines[i].first.first, lines[i].first.second,
                    lines[j].first.first, lines[j].first.second
                );
            // If this is the earliest intersection, record it
            // Note that we are over-writing previous tying intersections with
            // smaller slopes
            if(my_pos <= best_pos) {
                best_j = j;
                best_pos = my_pos;
            }
        }
        PRINT_DEBUG("Adding hull: " << prev_pos << ", " << best_pos << ", " << lines[i].second->ConvertToString() << endl, 6);
        hull.push_back(make_pair(make_pair(prev_pos, best_pos), lines[i].second));
        i = best_j;
        prev_pos = best_pos;
    }
    return hull;
}


// Calculate the n-best list giving the current weights
const ExamplePair & 
        TuningExampleNbest::CalculateModelHypothesis(Weights & weights) const {
    Real best_score = -REAL_MAX;
    const ExamplePair * best_pair = NULL;
    BOOST_FOREACH(const ExamplePair & exp_pair, nbest_) {
        Real score = weights * exp_pair.first;
        if(best_score < score) {
            best_score = score;
            best_pair = &exp_pair;
        }
    }
    if(best_pair == NULL) THROW_ERROR("Could not find best hypothesis in n-best");
    return *best_pair;
}
