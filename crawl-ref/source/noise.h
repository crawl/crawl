// Private header for shout.h.

#ifndef NOISE_H
#define NOISE_H

// [ds] The old noise system was pretty simple: noise level (loudness) ==
// distance covered. Since the new system considers terrain when propagating
// sound, using the same noise attenuation of 1 unit per square traveled would
// mean greatly reduced sound propagation on average.
//
// To compensate for sound being blocked by walls and doors, I've lowered the
// base attenuation from its original value of 1 (1000 millis).
//
// Note that this reduced attenuation makes open levels nastier, since there's
// nothing to block sound propagation there, and the lowered attenuation means
// sound goes farther.
//
const int BASE_NOISE_ATTENUATION_MILLIS = 850;
const int NOISE_ATTENUATION_COMPLETE = 250000;
const int LOWEST_AUDIBLE_NOISE_INTENSITY_MILLIS = 1000;

static inline int noise_is_audible(int noise_intensity_millis)
{
    return noise_intensity_millis >= LOWEST_AUDIBLE_NOISE_INTENSITY_MILLIS;
}

struct noise_t
{
    coord_def noise_source;

    string noise_player_msg;

    // Thousandths of noise intensity (i.e. the intensity passed to
    // noisy() * 1000)
    int noise_intensity_millis;

    int16_t noise_id;

    mid_t noise_producer_mid;

    noise_t(coord_def _noise_source = coord_def(),
            string _noise_player_msg = "",
            int _noise_intensity_millis = 0,
            mid_t _noise_producer_mid = MID_NOBODY,
            uint16_t _flags = 0)
        : noise_source(_noise_source),
          noise_player_msg(_noise_player_msg),
          noise_intensity_millis(_noise_intensity_millis),
          noise_id(-1),
          noise_producer_mid(_noise_producer_mid)
    {
    }

    bool silent() const
    {
        return !noise_is_audible(noise_intensity_millis);
    }

    bool operator < (const noise_t &other) const
    {
        return noise_intensity_millis < other.noise_intensity_millis;
    }
};

struct noise_cell
{
    // The cell from which the noise reached this cell (delta)
    coord_def neighbour_delta;

    int16_t noise_id;
    int noise_intensity_millis;
    int noise_travel_distance;

    noise_cell();
    bool can_apply_noise(int noise_intensity_millis) const;
    bool apply_noise(int noise_intensity_millis,
                     int noise_id,
                     int travel_distance,
                     const coord_def &neighbour_delta);

    bool silent() const
    {
        return !noise_is_audible(noise_intensity_millis);
    }

    // Given a destination cell adjacent to this cell, returns the
    // angle of <previous cell> <pos> <next-pos>:
    //  * If all three cells are in a straight line, returns 0
    //  * If the target cell is a knight's move from the original position,
    //    returns 1
    //  * If the target cell makes a right angle from the original position,
    //    returns 2
    //  * If the target cell makes a sharp 45 degree angle, returns 3.
    //  * If the original position is the same as the new position, returns 4.
    // This number is used to multiply the noise attenuation for noise passing
    // through this cell.
    int turn_angle(const coord_def &next_delta) const;
};

class noise_grid
{
public:
    noise_grid();

    // Register a noise on the noise grid. The noise will not actually
    // propagate until propagate_noise() is called.
    void register_noise(const noise_t &noise);

    // Propagate noise from the noise sources registered.
    void propagate_noise();

    // Clear all noise from the noise grid.
    void reset();

    bool dirty() const { return !noises.empty(); }

#ifdef DEBUG_NOISE_PROPAGATION
    void dump_noise_grid(const string &filename) const;
    void write_noise_grid(FILE *outf) const;
    void write_cell(FILE *outf, coord_def p, int ch) const;
#endif

private:
    bool propagate_noise_to_neighbour(int base_attenuation,
                                      int travel_distance,
                                      const noise_cell &cell,
                                      const coord_def &pos,
                                      const coord_def &next_position);
    void apply_noise_effects(const coord_def &pos,
                             int noise_intensity_millis,
                             const noise_t &noise,
                             int noise_travel_distance);

    coord_def noise_perceived_position(actor *act,
                                       const coord_def &affected_position,
                                       const noise_t &noise) const;

private:
    FixedArray<noise_cell, GXM, GYM> cells;
    vector<noise_t> noises;
    int affected_actor_count;
};

#endif
