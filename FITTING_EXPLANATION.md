# Square Root Fitting Process for PneuNet Pressure Calibration

## Overview
The system calibrates the relationship between capacitance change (ΔC) and pressure (P) using a **square root fit equation**:

$$P = a \cdot \sqrt{\Delta C - b} + c$$

This replaces the previous logarithmic fit approach for better pressure estimation and grasping detection.

## Why Square Root Fit?

### Physical Motivation
- **Square root** relationships appear naturally in sensor physics and elasticity problems
- Better captures **non-linear sensor response** while remaining computationally efficient
- Provides smooth, monotonic pressure prediction across the measurement range

### Advantages for ESP32
- **Fast computation**: `sqrtf()` is highly optimized on ARM processors
- **Numerically stable**: Unlike logarithm, no division by small numbers
- **Memory efficient**: No lookup tables needed
- **Lower dynamic range**: Avoids extreme values that logarithm can produce

## Calibration Process

### Step 1: Data Collection
During calibration, the system:
1. Records raw (ΔC, P) measurements as pressure is gradually applied
2. **Groups samples** in batches of 5 to average out noise and reduce outliers
3. Stores grouped samples: `calib_raw_samples[i] = {delta_c, pressure}`

**Key parameters**:
```cpp
const int MAX_CALIB_SAMPLES_PER_POINT = 1000;  // Maximum samples per delta-C
const int GROUP_SIZE = 5;                      // Samples per averaging group
```

### Step 2: Data Filtering
Before fitting, the system filters valid calibration points:
- **Minimum pressure**: `P > 0.2 kPa` (ensures data is in usable grasping range)
- **Valid capacitance**: `ΔC > 0 pF` (required for sqrt function)

This filtering focuses the calibration on the actual grasping detection range.

### Step 3: Shift Parameter Estimation
The algorithm estimates the shift parameter `b` which represents the minimum capacitance change below which pressure is zero:
- Finds the minimum ΔC from valid data points
- Uses 50% of this minimum as the shift offset: `b = 0.5 × min(ΔC)`

This prevents the square root from operating on zero or negative values and captures the physical threshold of the sensor.

### Step 4: Linear Regression on Transformed Space
The fitting transforms the equation into a linear regression problem:
- Let $x = \sqrt{\Delta C - b}$
- Then: $P = a \cdot x + c$ (standard linear form)

The algorithm computes:
$$a = \frac{n \sum(P \cdot \sqrt{\Delta C - b}) - \sum\sqrt{\Delta C - b} \cdot \sum P}{n \sum(\sqrt{\Delta C - b})^2 - (\sum\sqrt{\Delta C - b})^2}$$

$$c = \frac{\sum P - a \cdot \sum\sqrt{\Delta C - b}}{n}$$

Where $n$ = number of valid samples

### Step 5: Persistence
Fitted coefficients are saved to ESP32's NVS (Non-Volatile Storage):
```cpp
prefsPneunet.putFloat("curve_a", calib_curve_delta_c_a);  // a coefficient
prefsPneunet.putFloat("curve_b", calib_curve_delta_c_b);  // b (shift)
prefsPneunet.putFloat("curve_c", calib_curve_delta_c_c);  // c offset
```

## Runtime Usage

### Pressure Prediction
Once calibrated, predict expected pressure for any ΔC measurement:

```cpp
float getExpectedPressure(float delta_c) {
  if (calib_curve_points == 0) return 0.0f;
  
  // Clamp delta_c to ensure we stay above shift parameter b
  if (delta_c <= calib_curve_delta_c_b) 
    delta_c = calib_curve_delta_c_b + 0.01f;
  
  return calib_curve_delta_c_a * sqrtf(delta_c - calib_curve_delta_c_b) 
         + calib_curve_delta_c_c;
}
```

The calculation:
1. Clamps ΔC to stay above `b` to prevent negative values in sqrt
2. Computes the square root term: $\sqrt{\Delta C - b}$
3. Applies the linear transformation: $P = a \cdot \sqrt{\Delta C - b} + c$

### Grasping Detection
Compare actual pressure vs. expected pressure:
```
Grasping Threshold = Expected_Pressure + GRASPING_PRESSURE_BUFFER (0.3 kPa)
Is_Grasping = (Actual_Pressure > Grasping_Threshold)
```

If actual pressure exceeds expected by the buffer, an object is being grasped.

## Implementation Details for ESP32

### Memory Efficiency
- **No lookup tables**: Computation is inline via `sqrtf()`
- **Minimal storage**: Only 3 floats needed (`a`, `b`, and sample count)
- **Single pass**: Data is processed once during calibration

### Numerical Stability
1. **Clamping**: ΔC is clamped to 0.01 pF minimum to avoid numeric issues
2. **Regularization**: Filtering (P > 0.2 kPa) prevents fitting on noisy, low-pressure data
3. **Singularity check**: Detects and reports degenerate matrices

### Performance
- **Calibration**: O(n) single pass through samples
- **Runtime**: O(1) constant time per pressure prediction
- **CPU cost**: ~50 CPU cycles per prediction (one sqrt, two multiplications, one addition)

## Comparison: Square Root vs. Logarithmic

| Aspect | Square Root | Logarithmic |
|--------|------------|------------|
| Equation | $P = a\sqrt{\Delta C - b} + c$ | $P = a\ln(\Delta C) + b$ |
| Parameters | 3 (a, b, c) | 2 (a, b) |
| Shift parameter | Physical threshold | N/A |
| Computational cost | Low (optimized `sqrt`) | Medium (`log` is slower) |
| Numerical stability | Excellent (no division) | Good (can have issues near zero) |
| Physical interpretation | Elastic deformation with offset | Exponential sensor response |
| ESP32 suitability | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |

## Calibration Tips

1. **Gradual pressure increase**: Slowly increase pressure while measuring to get good coverage
2. **Full range**: Ensure calibration covers the full expected grasping pressure range
3. **Sample count**: Aim for 50-100 grouped samples (250-500 raw measurements) for robust fit
4. **Pressure stability**: Allow brief pauses to stabilize between pressure increments
5. **Verification**: After calibration, test on known object sizes to validate

## Example Calibration Output
```
PneuNet deformation calibration fitted (SQUARE ROOT): 
P = 2.145634*sqrt(ΔC - 5.2341) + -0.1234 (from 150 samples, 120 valid)
```

This means:
- For a given capacitance change ΔC: 
  1. Subtract the shift `b = 5.2341`
  2. Take the square root
  3. Multiply by `a = 2.146`
  4. Add the offset `c = -0.1234`
  
Example: If ΔC = 10.0 pF:
$$P = 2.146 \times \sqrt{10.0 - 5.2341} + (-0.1234) = 2.146 \times \sqrt{4.766} - 0.1234 \approx 4.65 \text{ kPa}$$
