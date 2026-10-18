#version 300 es

// Geometry, clipping and depth comparisons need consistent precision across GPUs.
precision highp float;
precision highp int;
precision lowp usampler2D;
