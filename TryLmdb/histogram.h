//It is Level DB test utils, only for test bench
#ifndef STORAGE_LEVELDB_UTIL_HISTOGRAM_H_
#define STORAGE_LEVELDB_UTIL_HISTOGRAM_H_

#include <string>
#include <stdio.h>
namespace leveldb {

	class Histogram {
	public:
		Histogram() { }
		~Histogram() { }

		void Clear();
		void Add(double value);
		void Merge(const Histogram& other);

		std::string ToString() const;

	private:
		double min_;
		double max_;
		double num_;
		double sum_;
		double sum_squares_;

		enum { kNumBuckets = 154 };
		static const double kBucketLimit[kNumBuckets];
		double buckets_[kNumBuckets];

		double Median() const;
		double Percentile(double p) const;
		double Average() const;
		double StandardDeviation() const;
	};
#define snprintf sprintf_s
}  // namespace leveldb

#endif // STORAGE_LEVELDB_UTIL_HISTOGRAM_H_