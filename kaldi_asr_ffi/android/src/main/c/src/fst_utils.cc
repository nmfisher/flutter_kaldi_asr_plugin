#define _GLIBCXX_USE_CXX11_ABI 0

#include <fst/fstlib.h>
#include <iostream>
#include <string>
#include <vector>
#include <random>

#include <fst/extensions/far/far.h>
#include <fst/flags.h>
#include <fst/script/compile.h>
#include <fstream>
#include <fst/script/getters.h>

using namespace fst;

StdVectorFst compile(std::string text) {
  StdVectorFst fst;
  int state = fst.AddState();   // 1st state will be state 0 (returned by AddState) 
  fst.SetStart(state);  // arg is state ID
  // split string by tab
  std::istringstream iss(text);
  std::string token;
  const char *delimiter = "\t";
  // for each token
  while (std::getline(iss, token, *delimiter)) {
    // increment state number
    state = fst.AddState();
    //fprintf(stderr, "Adding token %s in state %d", token.c_str(), state);
    int isym = stoi(token);
    fst.AddArc(state-1, StdArc(isym, isym, 0, state));  
  }     
  fst.SetFinal(state, 0);
//  fst.Write("compiled.fst");
  return fst;
}

//  
//  inputText is the tab-separated input symbol IDs (e.g. "0\t1"), where 0=你好,1=嗎
//  symbolFilePath is the filepath of the symbol file.
//  modelFilePath is the filepath of the FST file with which to compose the input
extern "C" const char* check(char *inputChars, char *modelFilePathChars)  {

  std::string inputString(inputChars);
  // Convert the input symbols to a binary FST 
  StdVectorFst input = compile(inputString);
  // Reads in the transduction model. 
  std::string modelFilePath = std::string(modelFilePathChars);
   
  StdVectorFst *model = StdVectorFst::Read(modelFilePath);
  ArcSort(&input, StdOLabelCompare());
  ArcSort(model, StdILabelCompare());
  
  // Container for composition result. 
  StdVectorFst result;
  // Creates the composed FST. 
  Compose(input, *model, &result);
//  result.Write("composed.fst");
  int state = result.Start();
  std::string output = "";
  if(state == -1) {
    std::cerr << "No viable paths found, input must be invalid.\n";
    return output.c_str();
  }
  int counter = 0;
  while(true) {
    ArcIterator< StdVectorFst > iter(result,state);
    StdArc arc;  
    arc = iter.Value();
    output += to_string(arc.olabel); 
    output += "\t";
    state = arc.nextstate;
    if(result.Final(state) != TropicalWeight::Zero()) {    
      break;
    }

    if(counter > 99999) {
      output = "INFINITE LOOP ERROR";
      break;
    }
    counter++;
  }
  return output.c_str(); 
} 

int _addArcs(StdVectorFst& fst, int state, std::stack<StdArc>& arcs, std::vector<string>& stems, int maxArcs, default_random_engine& rng, string stem) {
  // get the number of arcs in the state
  ArcIterator<StdVectorFst> iter(fst,state);
  std::vector<StdArc> localArcs;
  for(iter; !iter.Done(); iter.Next()) localArcs.push_back(iter.Value());
//  fprintf(stderr, "%d arcs in state %d\n", localArcs.size(), state);
  if(localArcs.size() == 0)
    return 0;

  std::shuffle(localArcs.begin(), localArcs.end(), rng);
  int added = 0;

  for(const auto& arc : localArcs) {
    if(added >= maxArcs)
      break;
    arcs.push(arc);
    stems.push_back(stem);
    added++;
  }
  
  return added;
}

void _generate(StdVectorFst& fst, vector<string>& accum, int maxArcs, default_random_engine& generator) {
  string stem;
  std::stack<StdArc> arcs;
  std::vector<string> stems;
  _addArcs(fst, fst.Start(), arcs, stems, maxArcs, generator, "");
  StdArc arc;
  while(true) { 
    if(arcs.size() == 0) {
//      fprintf(stderr, "No states left\n");
      break;
    }
//    fprintf(stderr, "%d arcs on stack\n", arcs.size());
//    fflush(stderr);
    arc = arcs.top();
    arcs.pop();
    stem = stems.back();
    stems.pop_back();
    
    if(arc.olabel != 0) {
      string symbol = to_string(arc.olabel);
      stem += symbol;
      stem += "\t";
//      fprintf(stderr, "Got symbol %s\n", symbol.c_str());
    } 
    // if this is a final state, add the current string
    if(fst.Final(arc.nextstate) != TropicalWeight::Zero()) {
      accum.push_back(stem.substr(0, stem.size() - 1));
      accum.push_back("\n");        
    } 
    int arcsAdded = _addArcs(fst, arc.nextstate, arcs, stems, maxArcs, generator, stem);
//    fprintf(stderr, "Added %d arcs for next state %d\n", arcsAdded, arc.nextstate);
//    fflush(stderr);
  }
}

extern "C" const char* generate(const char* modelFilePathChars, int maxArcs) {

  
  // Reads in the transduction model. 
  std::string modelFilePath = std::string(modelFilePathChars);
  StdVectorFst *model = StdVectorFst::Read(modelFilePath);
  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::default_random_engine generator(seed);
  std::vector<string> enumeration;
  _generate(*model, enumeration, maxArcs, generator);
  enumeration.pop_back(); // remove the last empty newline
  string output;
  for(const auto& item : enumeration)
    output += item;
//  std::cerr << output << endl;
  char *retval = (char *)malloc(output.length()+1);
  strcpy(retval, output.c_str());
  return retval;
}


int main(int argc, char** argv) { 
  //std::string src = "0\t1\t二\t二\n1\t2\t十\t十\n2";
 //std::cout << check(argv[1], argv[2]) << endl;
  //std::cout << generate(argv[1]);
}

