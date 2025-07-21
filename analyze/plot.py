import numpy as np
from src.scenario import (CLK_FREQ, CLK_PERIOD_sec, CLKS_per_PWM, Dir,
 TACH_PULSES_per_REV)

class HorizVert:

    """record a set of points to draw horizontal and vertical lines"""

    def __init__(self, max_points, debug=False):
        self.x = 0
        self.y = None
        self._points = np.empty(shape=(max_points, 2), dtype=np.int64)
        self.index = 0
        self.debug = debug

    def set_X(self, x, label='_'):
        if self.debug:
            print(f'{label}: X {self.x} -> {x}')
        assert x > self.x
        assert self.y is not None
        self._points[self.index] = [x, self.y]
        self.index += 1
        self.x = x

    def set_Y(self, y, label='_'):
        if self.debug:
            print(f'{label}: Y {self.y} -> {y}')
        assert 0 <= y <= CLKS_per_PWM
        self._points[self.index] = [self.x, y]
        self.index += 1
        self.y = y

    @property
    def points(self):
        return np.array(self._points[:self.index])


def make_points(signal, max_time=None, debug=False):

    def dbg_prt(*a, **kw):
        if (debug):
            print(*a, **kw)
            sys.stdout.flush()

    P = CLKS_per_PWM
    dir = signal.dir
    clk = signal.clk
    hv = HorizVert(3 * clk.size + 2, debug=debug)
    if clk.size == 0:
        # No edges.  Assume 100% PWM.
        hv.set_Y(P, 'A')
        hv.set_X(max_time, 'A')
        return hv.points
    if max_time is None:
        max_time = clk[-1]

    if dir[0] == Dir.FALLING:
        dir = np.insert(dir, 0, [Dir.RISING])
        clk = np.insert(clk, 0, [0])

    if dir[-1] == Dir.RISING:
        if clk[-1] < max_time - P:
            dir = np.append(dir, [Dir.FALLING])
            clk = np.append(clk, [max_time])
    dbg_prt(f'adjusted: {clk}')

    assert clk.size % 2 == 0
    prev_up = 0
    prev_wid = None
    for (up, dn) in np.split(clk, clk.size // 2):
        wid = dn - up
        dbg_prt(f'up={up} dn={dn} wid={wid} '
                f'prev_up={prev_up} prev_wid={prev_wid}')
        assert wid > 0

        if up - prev_up > 1.5 * P:
            # multi-cycle zero
            if prev_wid is not None:
                hv.set_X(prev_up + P, 'B')
            hv.set_Y(0, 'B')
            # hv.set_X(up, 'B')
            prev_wid = 0
            prev_up = up

        if wid > P:
            # multi-cycle pulse
            f_wid = wid % P
            i_wid = wid - f_wid
            if prev_wid is not None:
                hv.set_X(up, 'C')
            hv.set_Y(P, 'C')
            # hv.set_X(up + i_wid, 'C')
            # hv.set_Y(f_wid, 'C')
            up += i_wid
            wid = f_wid
            prev_wid = P

        #  if prev_wid is None:
        #     hv.set_Y(wid, 'D')
        if wid != prev_wid:
            if prev_wid is not None:
                hv.set_X(up, 'E')
            hv.set_Y(wid, 'E')
            prev_wid = wid
            
        prev_up = up
        # prev_wid = wid % P

    dbg_prt(f'END: max_time={max_time} prev_up={prev_up} prev_wid={prev_wid}')
    if max_time > prev_up + P:
        hv.set_X(prev_up + P, 'F')
        hv.set_Y(0, 'F')
        prev_up += P
    if max_time > prev_up:
        hv.set_X(max_time, 'F')

    return hv.points


def pwm_to_real_units(pwm_points):
    real_pts = pwm_points.astype(np.float64)
    real_pts[:,0] *= CLK_PERIOD_sec
    real_pts[:,1] /= CLKS_per_PWM
    return real_pts

def raw_tach_points(scenario):
    clk = scenario.tach.clk
    x = clk[1:] * CLK_PERIOD_sec
    y = CLK_FREQ / np.diff(clk) * 60 / 2 / TACH_PULSES_per_REV
    return np.vstack([x, y]).T
