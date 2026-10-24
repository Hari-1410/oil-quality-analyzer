#ifndef DECISION_TREE_H
#define DECISION_TREE_H

#include <stdint.h>

int32_t decision_tree_predict(const int16_t *features, int32_t features_length);

int decision_tree_predict_proba(
    const int16_t *features,
    int32_t features_length,
    float *out,
    int out_length);

#endif
