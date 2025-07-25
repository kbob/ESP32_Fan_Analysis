# Cascaded Integrator-Comb filter

from collections import namedtuple


class Intermediates(namedtuple('Intermediates',
                               'samples integrated decimated combed')):
    pass


class CICDecimator:

    def __init__(self, R, M, N):
        """Decimating low pass IIR filter.

           Keyword arguments:
           R -- decimation ratio
           M -- samples/stage
           N -- filter order
        """
        self.integrators = [Integrator() for i in range(N)]
        self.decimator = Decimator(R)
        self.combs = [Comb(M) for i in range(N)]

    def process(self, samples):

        # integrate
        integrated = samples
        for i in self.integrators:
            integrated = i.process(integrated)

        # decimate
        decimated = self.decimator.process(integrated)

        # comb
        combed = decimated
        for c in self.combs:
            combed = c.process(combed)
    
        print(f'{len(samples)=} {len(integrated)=}')
        self.intermediates = Intermediates(samples, integrated, decimated, combed)
        return combed


class CICPulseDecimator:

    def __init__(self, R, M, N):
        self.R = R
        self.N = N
        self.t0 = 0
        self.acc = [0] * N
        self.combs = [Comb(M) for i in range(N)]

    def process_pulses(self, pulses, n):

        decimated = []
        j = 0
        for t in range(self.t0, self.t0 + n, self.R):
            # t1 = t + self.R
            while j < len(pulses):
                tp = pulses[j] # time of pulse
                if tp >= t:
                    break
                print(f'{tp} - {self.t0}')
                self.acc[0] += tp - self.t0
                self.t0 = tp
                j += 1
            print(f'{t} - {self.t0} X')
            self.acc[0] += t - self.t0
            self.t0 = t
            decimated.append(self.acc[0])


        combed = decimated
        for c in self.combs:
            combed = c.process(combed)
        
        self.intermediates = Intermediates(None, None, decimated, combed)
        return combed


class Integrator:

    def __init__(self):
        self.acc = 0
    
    def process(self, samples):
        acc = self.acc
        out = []
        for x in samples:
            acc += x
            out.append(acc)
        self.acc = acc
        return out


class Decimator:

    def __init__(self, R):
        self.R = R
        self.remainder = 0

    def process(self, samples):
        R = self.R
        rem = self.remainder
        self.remainder = (rem + len(samples)) % R
        return samples[rem::R]


class Comb:

    def __init__(self, M):
        self.M = M
        self.mem = [0] * M

    def process(self, samples):
        M = self.M
        n = len(samples)
        ms = self.mem + list(samples)
        out = []
        for i in range(n):
            out.append(samples[i] - ms[i])
            # acc -= ms[i]
            # acc += samples[i]
            # out.append(acc)
        self.mem = ms[-M:]
        return out


# cic = CICDecimator(5, 2, 2)
# samples = [0] * 500
# samples[1::20] = [+1] * (500 // 20)
# samples[11::20] = [-1] * (500 // 20)
# # samples = [0] + ([+10] * 10 + [-10] * 10) * 25
# out = cic.process(samples)
# # print(ss(out))

