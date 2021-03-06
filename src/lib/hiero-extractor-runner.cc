#include <iostream>
#include <fstream>
#include <travatar/global-debug.h>
#include <travatar/tree-io.h>
#include <travatar/hiero-extractor.h>
#include <travatar/hiero-extractor-runner.h>
#include <travatar/config-hiero-extractor-runner.h>
#include <travatar/binarizer.h>
#include <travatar/rule-composer.h>
#include <travatar/rule-filter.h>
#include <travatar/hyper-graph.h>
#include <travatar/alignment.h>
#include <travatar/dict.h>
#include <boost/scoped_ptr.hpp>
#include <boost/foreach.hpp>

using namespace travatar;
using namespace std;
using namespace boost;

// Run the model
void HieroExtractorRunner::Run(const ConfigHieroExtractorRunner & config) {
    // Sanity Check
    IsSane(config);

    // Create the rule extractor
    HieroExtractor extractor;
    extractor.SetMaxInitialPhrase(config.GetInt("max_initial_phrase"));
    extractor.SetMaxTerminals(config.GetInt("max_terminals"));
    
    // Open the files
    const vector<string> & argv = config.GetMainArgs();
    ifstream src_in(argv[0].c_str());
    if(!src_in) THROW_ERROR("Could not find source file: " << argv[0]);
    ifstream trg_in(argv[1].c_str());
    if(!trg_in) THROW_ERROR("Could not find target file: " << argv[1]);
    ifstream alg_in(argv[2].c_str());
    if(!alg_in) THROW_ERROR("Could not find align file: " << argv[2]);
    
    string src_line,trg_line, align_line;

    // GLUE RULES
    // cout << "x0:S x1:X @ S ||| x0:S x1:X @ S ||| 1 ||| glue=1" << endl;
    // cout << "x0:X @ S ||| x0:X @ S ||| 1 ||| glue=1" << endl;

    // Rule Extraction Algorithm
    std::vector< vector<HieroRule*> > rules;
    long long int line = 0;
    while(true) {
        int has_src = getline(src_in,src_line) ? 1 : 0;
        int has_trg = getline(trg_in,trg_line) ? 1 : 0;
        int has_align = getline(alg_in,align_line) ? 1 : 0;
        
        if (has_src+has_trg+has_align == 0) break;
        if (has_src+has_trg+has_align != 3) {
            THROW_ERROR("File sizes don't match.");
        }

        Alignment alignment = Alignment::FromString(align_line);
        Sentence src_sent = Dict::ParseWords(src_line);
        Sentence trg_sent = Dict::ParseWords(trg_line);

        rules = extractor.ExtractHieroRule(alignment,src_sent,trg_sent);

        BOOST_FOREACH(vector<HieroRule*> rule , rules) {
            Real score = static_cast<Real>(1.0) / rule.size();
            BOOST_FOREACH(HieroRule* r , rule) {
                cout << r->ToString() << " ||| " << score << " ||| " << PrintAlignment(r->GetAlignments()) << endl;
                delete r;
            }
        }

        if (++line % 1000 == 0) {
            cerr << "Finished Processing: " << line << " lines. " << endl; 
        }
    }
}

string HieroExtractorRunner::PrintAlignment (const vector< pair<int,int> > & alignments) {
    ostringstream oss;
    bool first = true;
    pair<int,int> temp;
    BOOST_FOREACH(temp, alignments) {
        if (first) first = false;
        else oss << " ";
        oss << temp.first << "-" << temp.second;
    }
    return oss.str();
}

void HieroExtractorRunner::IsSane(const ConfigHieroExtractorRunner & config) 
{
    if (config.GetInt("max_initial_phrase") <= 0) {
        THROW_ERROR("max_initial_phrase must be greater than 0.");
    } 
    if (config.GetInt("max_terminals") <= 0) {
        THROW_ERROR("max_terminals must be greater than 0.");
    }
}
