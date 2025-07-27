# Extended Kalman Filter

Estimate velocity by applying way too much math.

## Resources



## Terms

### Constants
$I$ - identity matrix ($2 \times 2$, I think)
$$
    I = \begin{bmatrix}
        1 && 0 \\
        0 && 1
    \end{bmatrix}
$$
$\Delta\theta$ - radians per pulse

### Tuning Parameters
$R$ - measurement noise
$$
    R = \sigma_{T}^{2}
$$

$Q$ - process noise
$$
    Q = \begin{bmatrix}
        q_{\omega} && 0 \\
        0 && q_{\alpha}
    \end{bmatrix}
$$

### Inputs
$T_{k}$ - time interval

### Outputs
$RPM$ - rotational speed
$$
    RPM = \hat{\omega}_{k|k} \times \frac{60}{2\pi}
$$

### Filter State
$\hat{x}_{k|k}$ - *a posteriori* state estimate

$P_{k|k} $ - *a posteriori* covariance 

### Alphabetical:

$F_{k}$ - state transition matrix
$$
    F_{k} = \begin{bmatrix}
    1 & T_{k-1} \\
    0 & 1
\end{bmatrix}
$$

$K_{k}$ - Kalman gain
$$
K_{k} = P_{k|k-1} H_{k}^{T}S_{k}^{-1}
$$

$H_{k}$ - measurement Jacobian
$$
    H_{k} = \frac{\partial h}{\partial x}\bigg\vert_{\hat{x}_{k|k-1}}
    = \begin{bmatrix}
        \frac{\Delta\theta}{(\hat{\omega_{k|k-1}})^{2}} & 0
    \end{bmatrix}
$$

$I$ - identity matrix
$$
    I = \begin{bmatrix}
        1 && 0 \\
        0 && 1
\end{bmatrix}
$$

$P_{k}$ - predicted covariance
$$
    P_{0|0} = \begin{bmatrix}
        \sigma_{\omega}^{2} && 0 \\
        0 && \sigma_{\alpha}^{2}
    \end{bmatrix}
$$
$$
    P_{k|k-1} = F_{k} P_{k-1|k-1} F_{k}^{T}
$$

$Q$ - process noise
$$
    Q = \begin{bmatrix}
        q_{\omega} && 0 \\
        0 && q_{\alpha}
    \end{bmatrix}
$$

$R_{k}$ - measurement noise
$$
    R = \sigma_{T}^{2}
$$

$RPM$ - rotational speed
$$
    RPM = \hat{\omega}_{k|k} \times \frac{60}{2\pi}
$$

$S_{k}$ - innovation covariance
$$
    S_{k} = H_{k} P_{k|k-1} F_{k}^{T} + R
$$

$T_{k}$ - time interval

$h(x_{k})$ - measurement function
$$
    h(x_{k}) = \frac{\Delta\theta}{\omega_{k}}
$$

$\hat{x}_{k}$ - state variables, velocity and acceleration
$$
    \hat{x}_{k} = \begin{bmatrix}
        \omega_{k} \\
        \alpha_{k}
    \end{bmatrix}
$$

$y_{k}$ - innovation or measurement residual
$$
    y_{k} = z_{k} - h(\hat{x}_{k|k-1})
$$

$z_{k}$ - same as $T_{k}$

$\Delta\theta$ - radians per pulse

$\hat{\alpha}_{k}$ - acceleration

$\sigma_{a}$ - timer acceleration standard deviation

$\sigma_{\omega}$ - timer velocity standard deviation

$\hat{\omega}_{n}$ - velocity

