
#ifndef CAMERA_H
#define CAMERA_H

//==============================================================================================
// Originally written in 2016 by Peter Shirley <ptrshrl@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright and related
// and neighboring rights to this software to the public domain worldwide.
// This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public Domain Dedication
// along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include "hittable.h"
#include "material.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <omp.h>

class camera {
  public:
    double aspect_ratio      = 1.0;
    int    image_width       = 100;
    int    samples_per_pixel = 10;
    int    max_depth         = 10;

    double vfov     = 90;
    point3 lookfrom = point3(0,0,0);
    point3 lookat   = point3(0,0,-1);
    vec3   vup      = vec3(0,1,0);

    double defocus_angle = 0;
    double focus_dist    = 10.0;

    void render(const hittable& world) {
        initialize();

        // Allocate image storage outside the timed section.
        std::vector<color> image(
            static_cast<std::size_t>(image_width) * image_height
        );

        // Shared counter for the synchronization experiment.
        int pixels_done = 0;

        // LOCK FIX: Initialize the OpenMP lock.
        omp_lock_t pixel_lock;
        omp_init_lock(&pixel_lock);

        const auto start = std::chrono::steady_clock::now();

        // Parallel rendering using OpenMP.
        #pragma omp parallel for schedule(dynamic)
        for (int j = 0; j < image_height; j++) {
            for (int i = 0; i < image_width; i++) {

                // Deterministic random seed for each pixel.
                // Requires thread_local RNG in rtweekend.h.
                const std::uint32_t pixel_id =
                    static_cast<std::uint32_t>(j) *
                    static_cast<std::uint32_t>(image_width) +
                    static_cast<std::uint32_t>(i);

                const std::uint32_t pixel_seed =
                    42u ^ (pixel_id * 0x9e3779b9u);

                set_random_seed(pixel_seed);

                color pixel_color(0,0,0);

                for (int sample = 0; sample < samples_per_pixel; sample++) {
                    ray r = get_ray(i, j);

                    pixel_color += ray_color(r, max_depth, world);
                }

                // Each thread writes to a different pixel.
                image[static_cast<std::size_t>(j) * image_width + i]
                    = pixel_samples_scale * pixel_color;

                // LOCK FIX: Protect the shared counter update.
                omp_set_lock(&pixel_lock);
                pixels_done++;
                omp_unset_lock(&pixel_lock);
            }
        }

        // Destroy the lock after all threads have finished.
        omp_destroy_lock(&pixel_lock);

        const auto end = std::chrono::steady_clock::now();

        const double seconds =
            std::chrono::duration<double>(end - start).count();

        // Verify pixel counter correctness.
        std::clog << "Expected pixels: "
                  << image_width * image_height << "\n";

        std::clog << "Completed pixels: "
                  << pixels_done << "\n";

        // Image output is excluded from the rendering time.
        std::cout << "P3\n"
                  << image_width << ' ' << image_height
                  << "\n255\n";

        for (const color& pixel : image) {
            write_color(std::cout, pixel);
        }

        std::clog << "Render time: " << seconds << " seconds\n";
        std::clog << "Done.\n";
    }

  private:
    int    image_height;
    double pixel_samples_scale;
    point3 center;
    point3 pixel00_loc;
    vec3   pixel_delta_u;
    vec3   pixel_delta_v;
    vec3   u, v, w;
    vec3   defocus_disk_u;
    vec3   defocus_disk_v;

    void initialize() {
        image_height = int(image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        pixel_samples_scale = 1.0 / samples_per_pixel;

        center = lookfrom;

        auto theta = degrees_to_radians(vfov);
        auto h = std::tan(theta/2);

        auto viewport_height = 2 * h * focus_dist;
        auto viewport_width =
            viewport_height * (double(image_width)/image_height);

        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        vec3 viewport_u = viewport_width * u;
        vec3 viewport_v = viewport_height * -v;

        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / image_height;

        auto viewport_upper_left =
            center - (focus_dist * w) - viewport_u/2 - viewport_v/2;

        pixel00_loc =
            viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

        auto defocus_radius =
            focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));

        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    ray get_ray(int i, int j) const {
        auto offset = sample_square();

        auto pixel_sample = pixel00_loc
                          + ((i + offset.x()) * pixel_delta_u)
                          + ((j + offset.y()) * pixel_delta_v);

        auto ray_origin =
            (defocus_angle <= 0) ? center : defocus_disk_sample();

        auto ray_direction = pixel_sample - ray_origin;

        return ray(ray_origin, ray_direction);
    }

    vec3 sample_square() const {
        return vec3(random_double() - 0.5, random_double() - 0.5, 0);
    }

    vec3 sample_disk(double radius) const {
        return radius * random_in_unit_disk();
    }

    point3 defocus_disk_sample() const {
        auto p = random_in_unit_disk();

        return center
             + (p[0] * defocus_disk_u)
             + (p[1] * defocus_disk_v);
    }

    color ray_color(
        const ray& r,
        int depth,
        const hittable& world
    ) const {

        if (depth <= 0)
            return color(0,0,0);

        hit_record rec;

        if (world.hit(r, interval(0.001, infinity), rec)) {
            ray scattered;
            color attenuation;

            if (rec.mat->scatter(r, rec, attenuation, scattered))
                return attenuation * ray_color(
                    scattered, depth-1, world
                );

            return color(0,0,0);
        }

        vec3 unit_direction = unit_vector(r.direction());
        auto a = 0.5 * (unit_direction.y() + 1.0);

        return (1.0-a) * color(1.0, 1.0, 1.0)
             + a * color(0.5, 0.7, 1.0);
    }
};

#endif
