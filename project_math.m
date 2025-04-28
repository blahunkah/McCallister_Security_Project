% Harrison Blahunka
% 4/16/2024
% math for final project where we are making a security system thing with a servo, ultrasonic sensor and pic16f1619

SERVO_PWM_PERIOD = 20*10^3; % (us)

motor_period = 4; % (Second per Full Sweep Cycle)
ultrasonic_read_duration = 100; % (ms) Period ultrasonic sensor needs to make clean measurements, absolute min is 60 ms
num_averages_ultrasonic_reads = 4; % number of actual measurements that are averaged together to make an average measurement
field_of_view_min = 0; % (°)
field_of_view_max = 180; % (°)
detection_range_max = 120; % (in)
detection_range_min = 12; % (in)

duty_cycle_of_pos = [2.75, 12.5]; % these are the duty cycles that it took to get the maximum and minimum angles
CCP_of_pos = round(duty_cycle_of_pos/100 * 1024);
CCP_of_pos = [23, 83];

deg = 90;
deg_to_CCP = deg/180*(CCP_of_pos(2) - CCP_of_pos(1)) + CCP_of_pos(1);
deg_to_CCP1 = deg/180*60 + 23;
deg_to_CCP2 = (deg)/3 + 23;


%{

things that need to be calculated:
-time it takes to do one full averaged measurement
-how many angular steps can you do for a given range?
-what are the CCP values associated with these steps?
-beep step ranges?

%}

avg_read_time = ultrasonic_read_duration*num_averages_ultrasonic_reads; % (ms)
field_of_view = field_of_view_max - field_of_view_min; % (°)
num_motor_steps = 1000*motor_period/avg_read_time; % number of discrete steps the motor can take when running at a certain speed and measurement duration
step_values_deg = linspace(field_of_view_min, field_of_view_max, num_motor_steps); % (°)
step_values_hightime = (1390 - step_values_deg*6); % (us)
step_values_dutycycle = step_values_hightime./SERVO_PWM_PERIOD;
step_values_CCP = round(step_values_dutycycle*1024);
step_values_CCP1 = round((1390 - step_values_deg.*6)./20); % simplified calculation
step_values_CCP2 = round((69 - step_values_deg./3)); % simplified calculation
