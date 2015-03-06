// [[Rcpp::interfaces(r, cpp)]]
// [[Rcpp::plugins(openmp)]]
#include <local.h>
using namespace Rcpp;

void lookupOneChunk(const VecVecInt& vcdb, const VecVecInt& map,
		const VecVecInt::size_type num_comorbid,
		const VecVecInt::size_type begin, const VecVecInt::size_type end,
		ComorbidOut& chunk) {

#ifdef ICD9_DEBUG_TRACE
	std::cout << "lookupComorbidChunk begin = " << begin << ", end = " << end << "\n";
#endif
	const ComorbidOut falseComorbidChunk(num_comorbid * (1 + end - begin),
			false);
	chunk = falseComorbidChunk;
	// TODO: someday try looping through comorbidities in outside loop instead of inner loop.
	for (VecVecInt::size_type urow = begin; urow <= end; ++urow) { //end is index of end of chunk, so we include it in the loop.
#ifdef ICD9_DEBUG_TRACE
			std::cout << "lookupComorbidRangeOpenMP row: " << 1+urow-begin << " of " << 1+end-begin << "\n";
#endif
		for (VecVecInt::size_type cmb = 0; cmb < num_comorbid; ++cmb) { // loop through icd codes for this visitId
#ifdef ICD9_DEBUG_TRACE
				std::cout << "cmb = " << cmb << "\n";
#endif
			for (int nve = 0; nve < 3; ++nve) { // N, V, then E codes... argh
#ifdef ICD9_DEBUG_TRACE
					std::cout << "nve = " << nve << ". vcdb_x length = " << vcdb.size() << "\n";
#endif

				const VecInt& codes = vcdb[urow]; // these are the ICD-9 codes for the current visitid
				const VecInt& mapCodes = map[cmb];

				const VecInt::const_iterator cbegin = codes.begin();
				const VecInt::const_iterator cend = codes.end();
				for (VecInt::const_iterator code_it = cbegin; code_it != cend;
						++code_it) {
					bool found_it = std::binary_search(mapCodes.begin(),
							mapCodes.end(), *code_it);
					if (found_it) {
						const ComorbidOut::size_type chunk_idx = num_comorbid
								* (urow - begin) + cmb;
#ifdef ICD9_DEBUG
						chunk.at(chunk_idx) = true;
#endif
#ifndef ICD9_DEBUG
						chunk[chunk_idx] = true;
#endif
						nve = 3;
						break;
					} // end found_it
				} // end loop through codes in one comorbidity
			} // end loop N, V and E codes
		} // end loop through all comorbidities
	} // end loop through visits
#ifdef ICD9_DEBUG_TRACE
	std::cout << "finished with one chunk\n";
#endif
}

void lookupComorbidByChunkFor(const VecVecInt& vcdb, const VecVecInt& map,
		const VecVecInt::size_type chunkSize,
		const VecVecInt::size_type ompChunkSize, ComorbidOut& out) {
	const VecVecInt::size_type num_comorbid = map.size();
	const VecVecInt::size_type num_visits = vcdb.size();
	const VecVecInt::size_type last_i = num_visits - 1;
	VecVecInt::size_type chunk_end_i;
#ifdef ICD9_OPENMP
#pragma omp parallel shared(vcdb, map, out) private(chunk_end, chunk_out)
	omp_set_schedule(omp_sched_static, ompChunkSize); // ideally wouldn't repeat this over and over again
#ifdef ICD9_DEBUG_PARALLEL
	omp_sched_t* sched;
	int* threads;
	omp_get_schedule(sched, threads);
	std::out << "threads set = " << threads;
#endif
#pragma omp for ordered schedule(static)
#endif
	// loop through chunks at a time
	for (VecVecInt::size_type vis_i = 0; vis_i < num_visits;) {
#ifdef ICD9_DEBUG_TRACE
		std::cout << "vis_i = " << vis_i << " ";
#endif
		chunk_end_i = vis_i + chunkSize - 1; // chunk end is an index, so for zero-based vis_i and chunk_end should be the last index in the chunk
		if (chunk_end_i > last_i)
			chunk_end_i = last_i; // indices
		ComorbidOut chunk;
		lookupOneChunk(vcdb, map, num_comorbid, vis_i, chunk_end_i, chunk);
#pragma omp ordered
		{
#ifdef ICD9_DEBUG_TRACE
			std::cout << "writing a chunk beginning at: " << vis_i << "\n";
#endif
			std::copy(chunk.begin(), chunk.end(),
					out.begin() + (num_comorbid * vis_i));
		}
		vis_i += chunkSize;
	} // end parallel for
#ifdef ICD9_DEBUG
	std::cout << "finished looking up all chunks in for loop\n";
#endif
}

// just return the chunk results: this wouldn't cause invalidation of shared 'out'
ComorbidOut lookupComorbidByChunkFor(const VecVecInt& vcdb,
		const VecVecInt& map, const int chunkSize, const int ompChunkSize) {
	ComorbidOut out(vcdb.size() * map.size(), false);
	lookupComorbidByChunkFor(vcdb, map, chunkSize, ompChunkSize, out);
	return out;
}
