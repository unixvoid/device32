# Boids Simulation Parameters
Boids is an artificial life program, developed by Craig Reynolds in 1986, which simulates the flocking behaviour of birds, and related group motion.

## Core Simulation Parameters

- **NUM_BOIDS**
  - The total number of boids (birds) in the simulation. Higher numbers create more complex flocking patterns but require more processing power.

- **MAX_SPEED**
  - The maximum velocity a boid can reach. Limits how fast boids can move to prevent unrealistic speeds.

- **MAX_FORCE**
  - The maximum acceleration force that can be applied to a boid in a single frame. Prevents sudden, jerky movements.

## Flocking Behavior Distances

These parameters define the range of influence for each flocking behavior:

- **SEPARATION_DISTANCE**
  - The radius within which boids try to maintain distance from each other. Boids within this range will actively avoid crowding.

- **ALIGNMENT_DISTANCE**
  - The radius within which boids align their velocity with neighboring boids. Creates the "flying in the same direction" behavior.

- **COHESION_DISTANCE**
  - The radius within which boids are attracted to the center of mass of their neighbors. Creates the "sticking together" behavior.

## Flocking Behavior Weights

These parameters control the strength of each flocking behavior:

- **SEPARATION_WEIGHT**
  - How strongly boids avoid getting too close to each other. Higher values create more space between boids.

- **ALIGNMENT_WEIGHT**
  - How strongly boids try to match the velocity of their neighbors. Higher values create tighter formation flying.

- **COHESION_WEIGHT**
  - How strongly boids are attracted to the center of their group. Higher values keep the flock more tightly grouped.

## Edge Avoidance Parameters

- **EDGE_DISTANCE**
  - Distance from screen edges where boids begin to feel repulsion. Boids closer than this distance will steer away from the edge.

- **EDGE_WEIGHT**
  - Strength of the force pushing boids away from screen edges. Higher values create stronger avoidance behavior.

## Visual Parameters

- **BOID_TAIL_LENGTH**
  - Length of the directional tail drawn for each boid. Represents the boid's heading direction.

- **TRAIL_LENGTH**
  - Number of historical positions stored for each boid's trail. Creates the fading trail effect behind moving boids.

## Tuning Tips

- **Increasing separation weight** creates more spread-out flocks
- **Increasing alignment weight** creates more synchronized movement
- **Increasing cohesion weight** creates tighter, more compact flocks
- **Adjusting edge weight** affects how "bouncy" the boids appear near screen boundaries
- **Trail length** affects the visual persistence of boid movement paths