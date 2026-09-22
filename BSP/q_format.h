/**
 * @file q_format.h
 * @brief 无浮点依赖的 Q20 有符号定点数运算接口。
 *
 * q20_t 的低 Q20_FRACTION_BITS 位为小数部分。乘除和转换采用饱和算术；
 * q20_atan2()、q20_sin_cos() 采用 CORDIC 迭代，角度单位为弧度。
 */
#ifndef Q_FORMAT_H
#define Q_FORMAT_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Q20 定点数类型，低 20 位表示小数。 */
typedef int32_t q20_t;

/** @brief Q20 小数位数。 */
#define Q20_FRACTION_BITS 20U
#define Q20_SCALE         1048576LL

#define Q20_ZERO          ((q20_t)0)
#define Q20_ONE           ((q20_t)1048576)
#define Q20_TWO           ((q20_t)2097152)
#define Q20_HALF          ((q20_t)524288)
#define Q20_PI            ((q20_t)3294199)
#define Q20_HALF_PI       ((q20_t)1647099)
#define Q20_TWO_PI        ((q20_t)6588397)
#define Q20_RAD2DEG       ((q20_t)60078979)
#define Q20_DEG2RAD       ((q20_t)18301)
#define Q20_100           ((q20_t)104857600)
#define Q20_1000          ((q20_t)1048576000)
#define Q20_1E6           ((q20_t)INT32_MAX)
#define Q20_MAX           INT32_MAX
#define Q20_MIN           INT32_MIN

/** @brief 将 64 位中间结果饱和到 q20_t 的可表示范围。 */
static inline q20_t q20_saturate_i64(int64_t value)
{
    if (value > INT32_MAX) return INT32_MAX;
    if (value < INT32_MIN) return INT32_MIN;
    return (q20_t)value;
}

/** @brief 将整数转换为 Q20，超出范围时饱和。 */
static inline q20_t q20_from_i32(int32_t value)
{
    return q20_saturate_i64((int64_t)value * Q20_SCALE);
}

/** @brief Convert a double to Q20 with rounding and saturation. */
static inline q20_t q20_from_double(double value)
{
    const double q20_max = ((double)INT32_MAX - 0.5) / (double)Q20_SCALE;
    const double q20_min = ((double)INT32_MIN + 0.5) / (double)Q20_SCALE;
    double scaled;

    if (value >= q20_max)
    {
        return Q20_MAX;
    }
    if (value <= q20_min)
    {
        return Q20_MIN;
    }

    scaled = value * (double)Q20_SCALE;
    scaled += (scaled >= 0.0) ? 0.5 : -0.5;
    return (q20_t)scaled;
}

static inline q20_t q20_from_ratio(int32_t numerator, int32_t denominator)
{
    if (denominator == 0) return (numerator >= 0) ? Q20_MAX : Q20_MIN;
    return q20_saturate_i64(((int64_t)numerator * Q20_SCALE) / denominator);
}

static inline q20_t q20_mpy(q20_t a, q20_t b)
{
    return q20_saturate_i64(((int64_t)a * b) >> Q20_FRACTION_BITS);
}

static inline q20_t q20_div(q20_t a, q20_t b)
{
    if (b == 0) return (a >= 0) ? Q20_MAX : Q20_MIN;
    return q20_saturate_i64(((int64_t)a * Q20_SCALE) / b);
}

static inline uint64_t q20_isqrt_u64(uint64_t value)
{
    uint64_t root = 0U;
    uint64_t bit = 1ULL << 62;

    while (bit > value) bit >>= 2U;
    while (bit != 0U) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1U) + bit;
        } else {
            root >>= 1U;
        }
        bit >>= 2U;
    }
    return root;
}

static inline q20_t q20_sqrt(q20_t value)
{
    if (value <= 0) return Q20_ZERO;
    return q20_saturate_i64((int64_t)q20_isqrt_u64((uint64_t)(uint32_t)value << Q20_FRACTION_BITS));
}

static inline q20_t q20_atan2(q20_t y, q20_t x)
{
    static const q20_t atan_table[] = {
        823550, 486170, 256879, 130396, 65451, 32757, 16383, 8192, 4096,
        2048, 1024, 512, 256, 128, 64, 32, 16, 8, 4
    };
    int64_t vx = x;
    int64_t vy = y;
    int64_t angle = 0;
    uint32_t index;

    if ((vx == 0) && (vy == 0)) return Q20_ZERO;
    if (vx < 0) {
        vx = -vx;
        vy = -vy;
        angle = (y >= 0) ? Q20_PI : -Q20_PI;
    }
    if (vy == 0) return (q20_t)angle;

    for (index = 0U; index < (sizeof(atan_table) / sizeof(atan_table[0])); index++) {
        int64_t shifted_x = vx >> index;
        int64_t shifted_y = vy >> index;
        if (vy > 0) {
            vx += shifted_y;
            vy -= shifted_x;
            angle += atan_table[index];
        } else {
            vx -= shifted_y;
            vy += shifted_x;
            angle -= atan_table[index];
        }
    }
    return q20_saturate_i64(angle);
}

static inline void q20_sin_cos(q20_t value, q20_t *sin_value, q20_t *cos_value)
{
    static const q20_t atan_table[] = {
        823550, 486170, 256879, 130396, 65451, 32757, 16383, 8192, 4096,
        2048, 1024, 512, 256, 128, 64, 32, 16, 8, 4
    };
    int64_t angle = value % Q20_TWO_PI;
    int64_t x = 636751;
    int64_t y = 0;
    int32_t cosine_sign = 1;
    uint32_t index;

    if (angle > Q20_PI) angle -= Q20_TWO_PI;
    if (angle < -Q20_PI) angle += Q20_TWO_PI;
    if (angle > Q20_HALF_PI) {
        angle = Q20_PI - angle;
        cosine_sign = -1;
    } else if (angle < -Q20_HALF_PI) {
        angle = -Q20_PI - angle;
        cosine_sign = -1;
    }

    for (index = 0U; index < (sizeof(atan_table) / sizeof(atan_table[0])); index++) {
        int64_t shifted_x = x >> index;
        int64_t shifted_y = y >> index;
        if (angle >= 0) {
            x -= shifted_y;
            y += shifted_x;
            angle -= atan_table[index];
        } else {
            x += shifted_y;
            y -= shifted_x;
            angle += atan_table[index];
        }
    }

    if (sin_value != NULL) *sin_value = q20_saturate_i64(y);
    if (cos_value != NULL) *cos_value = q20_saturate_i64((int64_t)cosine_sign * x);
}

static inline q20_t q20_sin(q20_t value)
{
    q20_t result;
    q20_sin_cos(value, &result, NULL);
    return result;
}

static inline q20_t q20_cos(q20_t value)
{
    q20_t result;
    q20_sin_cos(value, NULL, &result);
    return result;
}

static inline q20_t q20_sat(q20_t value, q20_t high, q20_t low)
{
    if (value > high) return high;
    if (value < low) return low;
    return value;
}

static inline int32_t q20_to_i32(q20_t value)
{
    return value >> Q20_FRACTION_BITS;
}

static inline int32_t q20_mpy_i32_to_i32(q20_t value, int32_t multiplier)
{
    return (int32_t)(((int64_t)value * multiplier) >> Q20_FRACTION_BITS);
}

static inline q20_t q20_mpy_i32(int32_t multiplier, q20_t value)
{
    return q20_saturate_i64((int64_t)multiplier * value);
}

static inline q20_t abs_q20(q20_t value)
{
    return (value == INT32_MIN) ? INT32_MAX : ((value < 0) ? -value : value);
}

#define Q20fromI32(x)       q20_from_i32((x))
#define Q20(x)              q20_from_double((double)(x))
#define Q20divI32(n, d)     q20_from_ratio((n), (d))
#define Q20mpy(a, b)        q20_mpy((a), (b))
#define Q20div(a, b)        q20_div((a), (b))
#define Q20sqrt(x)          q20_sqrt((x))
#define Q20atan2(y, x)      q20_atan2((y), (x))
#define Q20sin(x)           q20_sin((x))
#define Q20cos(x)           q20_cos((x))
#define Q20sat(a, h, l)     q20_sat((a), (h), (l))
#define Q20int(x)           q20_to_i32((x))
#define Q20mpyI32toInt(v,m) q20_mpy_i32_to_i32((v), (m))
#define Q20mpyI32(m,v)      q20_mpy_i32((m), (v))

#endif /* Q_FORMAT_H */
