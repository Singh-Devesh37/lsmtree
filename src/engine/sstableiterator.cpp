#include "sstableiterator.h"

SSTableIterator::SSTableIterator(const std::string& filename){
    file.open(filename);
    next();
}

void SSTableIterator::next(){
    if(std::string line; std::getline(file,line)){
        if(std::istringstream iss(line); std::getline(iss,currKey,'\t') && std::getline(iss,currValue)){
            valid = true;
            return;
        }
        
    }
    valid = false;
}

bool SSTableIterator::isValid() const{
    return valid;
}