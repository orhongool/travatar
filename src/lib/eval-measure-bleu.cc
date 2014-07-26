
#include <travatar/eval-measure-bleu.h>
#include <travatar/global-debug.h>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <cmath>

using namespace std;
using namespace travatar;
using namespace boost;

EvalMeasureBleu::NgramStats * EvalMeasureBleu::ExtractNgrams(const Sentence & sentence) const {
    NgramStats * all_ngrams = new NgramStats;
    vector<WordId> ngram;
    for (int k = 0; k < ngram_order_; k++) {
        for(int i =0; i < max((int)sentence.size()-k,0); i++) {
            for ( int j = i; j<= i+k; j++) {
                ngram.push_back(sentence[j]);
            }
            ++((*all_ngrams)[ngram]);
            ngram.clear();
        }
    }
    return all_ngrams;
}

shared_ptr<EvalMeasureBleu::NgramStats> EvalMeasureBleu::GetCachedStats(const Sentence & sent, int cache_id) {
    if(cache_id == INT_MAX) return shared_ptr<NgramStats>(ExtractNgrams(sent));
    StatsCache::const_iterator it = cache_.find(cache_id);
    if(it == cache_.end()) {
        shared_ptr<NgramStats> new_stats(ExtractNgrams(sent));
        cache_.insert(make_pair(cache_id, new_stats));
        return new_stats;
    } else {
        return it->second;
    }
}

shared_ptr<EvalStats> EvalMeasureBleu::CalculateStats(const Sentence & ref, const Sentence & sys) const {
    shared_ptr<NgramStats> ref_s(ExtractNgrams(ref)), sys_s(ExtractNgrams(sys));
    return CalculateStats(*ref_s, ref.size(), *sys_s, sys.size());
}

// Measure the score of the sys output according to the ref
shared_ptr<EvalStats> EvalMeasureBleu::CalculateStats(const Sentence & ref, const Sentence & sys, int ref_cache_id, int sys_cache_id) {
    return CalculateStats(*GetCachedStats(ref, ref_cache_id), ref.size(), *GetCachedStats(sys, sys_cache_id), sys.size());
}

shared_ptr<EvalStats> EvalMeasureBleu::CalculateStats(const NgramStats & ref_ngrams, int ref_len,
                                                      const NgramStats & sys_ngrams, int sys_len) const {
    int vals_n = 3*ngram_order_;
    vector<EvalStatsDataType> vals(vals_n);

    for (int i =0; i<ngram_order_; i++) {
        vals[3*i] = 0;
        vals[3*i+1] = max(sys_len-i,0);
        vals[3*i+2] = max(ref_len-i,0);
    }

    for (NgramStats::const_iterator it = sys_ngrams.begin(); it != sys_ngrams.end(); it++) {
        NgramStats::const_iterator ref_it = ref_ngrams.find(it->first);
        if(ref_it != ref_ngrams.end()) {
            vals[3* (it->first.size()-1)] += min(ref_it->second,it->second);
        }
    }
    // Create the stats for this sentence
    EvalStatsPtr ret(new EvalStatsBleu(vals, smooth_val_, prec_weight_, mean_));
    // If we are using sentence based, take the average immediately
    if(scope_ == SENTENCE)
        ret = EvalStatsPtr(new EvalStatsAverage(ret->ConvertToScore()));
    return ret;
}

// Read in the stats
shared_ptr<EvalStats> EvalMeasureBleu::ReadStats(const std::string & line) {
    EvalStatsPtr ret;
    if(scope_ == SENTENCE)
        ret.reset(new EvalStatsAverage);
    else
        ret.reset(new EvalStatsBleu(std::vector<EvalStatsDataType>(), smooth_val_, prec_weight_, mean_));
    ret->ReadStats(line);
    return ret;
}

double EvalStatsBleu::GetAvgLogPrecision() const {
    int ngram_order = GetNgramOrder();
    double log_prec = 0.0;
    // Calculate the precision for each order
    for (int i=0; i < ngram_order; i++) {
        double smooth = (i == 0 ? 0 : smooth_);
        double num = (vals_[3*i]+smooth);
        double denom = (vals_[3*i+1]+smooth);
        double prec = (denom ? num/denom : 0);
        log_prec += (prec ? log(prec) : -DBL_MAX);
    }
    log_prec /= ngram_order;
    return log_prec;
}

double EvalStatsBleu::GetLengthRatio() const {
    return vals_[1]/vals_[2];
}

BleuReport EvalStatsBleu::CalcBleuReport() const {
    BleuReport report;
    double log_bleu = (mean_ == GEOMETRIC ? 1.0 : 0.0);
    int ngram_order = GetNgramOrder();
    // Calculate the precision for each order
    for (int i=0; i < ngram_order; i++) {
        double smooth = (i == 0 ? 0 : smooth_);
        double mat = (vals_[3*i]+smooth);
        double sys = (vals_[3*i+1]+smooth);
        double ref = (vals_[3*i+2]+smooth);
        double fmeas = mat / (prec_weight_ * sys + (1-prec_weight_) * ref);
        // cerr << "i="<<i<<", mat="<<mat<<", sys="<<sys<<", ref="<<ref<<", fmeas="<<fmeas<<", pw="<<prec_weight_<<endl;
        report.scores.push_back(fmeas);
        if(mean_ == GEOMETRIC)
            log_bleu *= fmeas;
        else
            log_bleu += fmeas;
    }
    // Average and convert to log scale
    if(log_bleu == 0)
        log_bleu = -DBL_MAX;
    else if(mean_ == GEOMETRIC)
        log_bleu = log(log_bleu)/4;
    else
        log_bleu = log(log_bleu/4);
    // vals_[vals__n-1] is the ref length, vals_[1] is the test length
    report.sys_len = vals_[1];
    report.ref_len = vals_[2];
    // Calculate the brevity penalty
    report.ratio = (double)report.sys_len/report.ref_len;
    double log_bp = 1.0-(double)report.ref_len/report.sys_len;
    if (log_bp < 0.0) {
        log_bleu += log_bp;
        report.brevity = exp(log_bp);
    } else {
        report.brevity = 1.0;
    }
    // Sanity check
    if(log_bleu > 0) THROW_ERROR("Found a BLEU larger than one: " << exp(log_bleu))
    report.bleu = exp(log_bleu);
    return report;
}


std::string EvalStatsBleu::ConvertToString() const {
    BleuReport report = CalcBleuReport();
    ostringstream oss;
    oss << "BLEU = " << report.bleu << ", " << report.scores[0];
    for(int i = 1; i < (int)report.scores.size(); i++)
        oss << "/" << report.scores[i];
    oss << " (BP=" << report.brevity << ", ratio=" << report.ratio << ", hyp_len=" << report.sys_len << ", ref_len=" << report.ref_len << ")";
    return oss.str();
}

double EvalStatsBleu::ConvertToScore() const {
    return CalcBleuReport().bleu;
}


EvalMeasureBleu::EvalMeasureBleu(const std::string & config) : ngram_order_(4), smooth_val_(0), scope_(CORPUS), prec_weight_(1.0), mean_(GEOMETRIC) {
    if(config.length() == 0) return;
    BOOST_FOREACH(const EvalMeasure::StringPair & strs, EvalMeasure::ParseConfig(config)) {
        if(strs.first == "order") {
            ngram_order_ = boost::lexical_cast<int>(strs.second);
        } else if(strs.first == "smooth") {
            smooth_val_ = boost::lexical_cast<double>(strs.second);
        } else if(strs.first == "prec") {
            prec_weight_ = boost::lexical_cast<double>(strs.second);
        } else if(strs.first == "factor") {
            factor_ = boost::lexical_cast<int>(strs.second);
        } else if(strs.first == "scope") {
            if(strs.second == "corpus") {
                scope_ = CORPUS;
            } else if(strs.second == "sentence") {
                scope_ = SENTENCE;
            } else {
                THROW_ERROR("Bad BLEU scope: " << config);
            }
        } else if(strs.first == "mean") {
            if(strs.second == "geom") {
                mean_ = GEOMETRIC;
            } else if(strs.second == "arith") {
                mean_ = ARITHMETIC;
            } else {
                THROW_ERROR("Bad BLEU scope: " << config);
            }
        } else {
            THROW_ERROR("Bad configuration string: " << config);
        }
    }
}
