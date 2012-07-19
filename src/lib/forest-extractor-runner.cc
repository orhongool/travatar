#include <travatar/util.h>
#include <travatar/tree-io.h>
#include <travatar/rule-extractor.h>
#include <travatar/forest-extractor-runner.h>
#include <travatar/binarizer-directional.h>
#include <boost/scoped_ptr.hpp>

using namespace travatar;
using namespace std;
using namespace boost;

// Run the model
void ForestExtractorRunner::Run(const ConfigForestExtractorRunner & config) {
    // Create the tree parser and rule extractor
    scoped_ptr<TreeIO> tree_io;
    if(config.GetString("input_format") == "penn")
        tree_io.reset(new PennTreeIO);
    else
        THROW_ERROR("Invalid TreeIO type: " << config.GetString("input_format"));
    ForestExtractor extractor;
    // Create the binarizer
    shared_ptr<GraphTransformer> binarizer;
    if(config.GetString("binarize") == "left") {
        binarizer.reset(new BinarizerDirectional(BinarizerDirectional::BINARIZE_LEFT));
    } else if(config.GetString("binarize") == "right") {
        binarizer.reset(new BinarizerDirectional(BinarizerDirectional::BINARIZE_RIGHT));
    } else if(config.GetString("binarize") != "none") {
        THROW_ERROR("Invalid binarizer type " << config.GetString("binarizer"));
    }
    // Open the files
    const vector<string> & argv = config.GetMainArgs();
    ifstream src_in(argv[0].c_str());
    if(!src_in) THROW_ERROR("Could not find src file: " << argv[0]);
    ifstream trg_in(argv[1].c_str());
    if(!trg_in) THROW_ERROR("Could not find trg file: " << argv[1]);
    ifstream align_in(argv[2].c_str());
    if(!align_in) THROW_ERROR("Could not find align file: " << argv[2]);
    // Get the lines
    string src_line, trg_line, align_line;
    int has_src, has_trg, has_align;
    int sent = 0;
    cerr << "Extracting rules (.=10,000, !=100,000 sentences)" << endl;
    while(true) {
        // Load one line from each file and check that they all exist
        has_src = getline(src_in, src_line) ? 1 : 0;
        has_trg = getline(trg_in, trg_line) ? 1 : 0;
        has_align = getline(align_in, align_line) ? 1 : 0;
        if(has_src + has_trg + has_align == 0) break;
        if(has_src + has_trg + has_align != 3)
            THROW_ERROR("File sizes don't match: src="<<has_src
                        <<", trg="<<has_trg<<", align="<<has_align);
        // Parse into the appropriate data structures
        istringstream src_iss(src_line);
        scoped_ptr<HyperGraph> src_graph(tree_io->ReadTree(src_iss));
        // Binarizer if necessary
        if(binarizer.get() != NULL) {
            scoped_ptr<HyperGraph> bin_graph(binarizer->TransformGraph(*src_graph));
            src_graph.swap(bin_graph);
        }
        // Get target words and alignment
        Sentence trg_sent = Dict::ParseWords(trg_line);
        Alignment align = Alignment::FromString(align_line);
        // Do the rule extraction
        shared_ptr<HyperGraph> rule_graph(
            extractor.ExtractMinimalRules(*src_graph, align));
        // TODO: do other processing
        // Print each of the rules
        BOOST_FOREACH(HyperEdge* edge, rule_graph->GetEdges())
            cout << extractor.RuleToString(*edge, 
                                           src_graph->GetWords(), 
                                           trg_sent) << endl;
        sent++;
        if(sent % 10000 == 0) {
            cerr << (sent % 100000 == 0 ? '!' : '.'); cerr.flush();
        }
    }
    cerr << endl;
}