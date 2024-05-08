/*
 * Copyright ©2024 Hannah C. Tang.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2024 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include "./QueryProcessor.h"

#include <algorithm>
#include <iostream>
#include <list>
#include <string>
#include <vector>

extern "C" {
#include "./libhw1/CSE333.h"
}

using std::list;
using std::sort;
using std::string;
using std::vector;

namespace hw3 {

// Modifies dst such that only elements with the same docID as those in 2 remain
// Additionally the rank of remaining docIDs increases by the corresponding rank
// in src
//
// e.g. if dst[i].docID equals src[j].docID for some j => dst[i].rank +=
// src[j].rank
//      if dst[i].docID has no matching docID in src, then remove dst[i]
static void MergeDocIDElementLists(list<DocIDElementHeader>& dst,
                                   const list<DocIDElementHeader>& src);

QueryProcessor::QueryProcessor(const list<string>& index_list, bool validate) {
  // Stash away a copy of the index list.
  index_list_ = index_list;
  array_len_ = index_list_.size();
  Verify333(array_len_ > 0);

  // Create the arrays of DocTableReader*'s. and IndexTableReader*'s.
  dtr_array_ = new DocTableReader*[array_len_];
  itr_array_ = new IndexTableReader*[array_len_];

  // Populate the arrays with heap-allocated DocTableReader and
  // IndexTableReader object instances.
  list<string>::const_iterator idx_iterator = index_list_.begin();
  for (int i = 0; i < array_len_; i++) {
    FileIndexReader fir(*idx_iterator, validate);
    dtr_array_[i] = fir.NewDocTableReader();
    itr_array_[i] = fir.NewIndexTableReader();
    idx_iterator++;
  }
}

QueryProcessor::~QueryProcessor() {
  // Delete the heap-allocated DocTableReader and IndexTableReader
  // object instances.
  Verify333(dtr_array_ != nullptr);
  Verify333(itr_array_ != nullptr);
  for (int i = 0; i < array_len_; i++) {
    delete dtr_array_[i];
    delete itr_array_[i];
  }

  // Delete the arrays of DocTableReader*'s and IndexTableReader*'s.
  delete[] dtr_array_;
  delete[] itr_array_;
  dtr_array_ = nullptr;
  itr_array_ = nullptr;
}

// This structure is used to store a index-file-specific query result.
typedef struct {
  DocID_t doc_id;  // The document ID within the index file.
  int rank;        // The rank of the result so far.
} IdxQueryResult;

vector<QueryProcessor::QueryResult> QueryProcessor::ProcessQuery(
    const vector<string>& query) const {
  Verify333(query.size() > 0);

  // STEP 1.
  // (the only step in this file)
  vector<QueryProcessor::QueryResult> final_result;

  for (uint32_t i = 0; i < index_list_.size(); i++) {
    uint32_t start = 0;
    DocIDTableReader* first_ditr = nullptr;
    while (first_ditr == nullptr && start < query.size()) {
      first_ditr = itr_array_[i]->LookupWord(query.at(start));
      start++;
    }
    if (first_ditr == nullptr) {
      continue;
    }
    list<DocIDElementHeader> final_id_rank_list = first_ditr->GetDocIDList();
    delete first_ditr;

    for (uint32_t j = start; j < query.size(); j++) {
      DocIDTableReader* curr_ditr = itr_array_[i]->LookupWord(query.at(j));
      if (curr_ditr == nullptr) {
        final_id_rank_list.clear();
        break;
      }
      list<DocIDElementHeader> curr_id_rank_list = curr_ditr->GetDocIDList();
      MergeDocIDElementLists(final_id_rank_list, curr_id_rank_list);
      delete curr_ditr;
    }

    for (auto& header : final_id_rank_list) {
      string file_name;
      if (dtr_array_[i]->LookupDocID(header.doc_id, &file_name)) {
        QueryResult qr;
        qr.rank = header.num_positions;
        qr.document_name = file_name;
        final_result.push_back(qr);
      }
    }
  }

  // Sort the final results.
  sort(final_result.begin(), final_result.end());
  return final_result;
}

static void MergeDocIDElementLists(list<DocIDElementHeader>& dst,
                                   const list<DocIDElementHeader>& src) {
  bool matched;
  for (auto it = dst.begin(); it != dst.end();) {
    matched = false;
    for (auto& header : src) {
      if (header.doc_id == (*it).doc_id) {
        (*it).num_positions += header.num_positions;
        matched = true;
      }
    }
    if (matched) {
      it++;
    } else {
      it = dst.erase(it);
    }
  }
}

}  // namespace hw3
