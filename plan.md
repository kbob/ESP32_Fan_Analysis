# Plan

- separate reading from fixing
- make an API: read_trace, fix_trace
- add more verbose-mode messages
- write another notebook that reads traces and graphs them
- start on &Delta;&Sigma; filter/decimator

# &Delta;&Sigma; Filter

- Separate streams for rising and falling tach edges
- Data type : u32?
- One calculation per edge:
   acc -= time since last edge
   acc += 0x8000_0000
- clamp subtraction to 0
- clamp addition to 0xFFFF_FFFF?

# CIC Filter

- R: ratio of sample rates
  - choose useful output sample rate.
  - maybe 80,000?  (1 KHz output Fs)
- M: moving average length
  - could be large so N can be small
- N: number of stages
  - -13dB filtering per stage
  - maybe 2nd or 3rd order?
  - no, needs to be enough to spread each pulse to survive resampling
- do it the hard way: one calc per AHB clock
- then optimize: One calc per edge
- max speed ~4200 RPM = 70 Hz, so 140 edges/sec = 7.1 msec/edge
  - 3.6 msec/edge counting both directions
- then feed it through an IIR filter.  (What order?)
- tune by adjusting output Fs, CIC order, IIR order and Fc.

# Ad-Hoc Filter

- Two streams, one for rising and one for falling edges
- At each edge, set the instantaneous speed to 1/last_interval.
- After x time without an edge, set the instantantaneous speed to 0.
  - x = 0.2 seconds?
- To filter, do the IIR calc, but the IIR calc recalculates α based on the current interval.

-  $ 1 - α = (1 - α_{0})^{\Delta t} $ <br> $ y_n = α x_n + (1 - α)x_{n-1} $

Try $α_0$ in the range 0.1 - 100 Hz.

```python
>>> from math import prod
>>> bits = lambda n: [e for e in (2**i for i in range(n.bit_length())) if n & e]
>>> 
>>> exn = lambda x, n: prod(x**i for i in bits(n))
>>> 
>>> exn(0.99, 13)
0.8775210229989678
>>> 
```
