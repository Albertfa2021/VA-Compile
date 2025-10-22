function DataHistoryVsMotionModelTest(sTrajectoryName)

if nargin == 0
    sTrajectoryName = 'ParabolicTrajectory';
end

%%
lineWidth = 1.5;

%%
logFileFolder = fileparts(fileparts( mfilename('fullpath') ));
baseFilename = fullfile(logFileFolder, sTrajectoryName);
historyBaseFilename = [baseFilename 'DataHistory'];
motionModelBaseFilename = [baseFilename 'MotionModel'];

%%
motionInput = loadMotionModelLog([motionModelBaseFilename '_Input.log']);
motionOutput = loadMotionModelLog([motionModelBaseFilename '_Estimated.log']);

historyInput = loadDataHistoryLog([historyBaseFilename '_Input.log']);
historyOutput = loadDataHistoryLog([historyBaseFilename '_Output.log']);

%% Check input data
inputPosDiff = motionInput.pos - historyInput.data;
if any( inputPosDiff(:)~= 0)
    warning('Input data between motion model and data history does not match');
end

%% Plot 3D Trajectory
f = figure;
ax = axes(f);
hold(ax, 'on');
plot3(historyInput.data(:,1), historyInput.data(:, 2), historyInput.data(:, 3), '-kx', 'linewidth', 1.25);
plot3(motionOutput.pos(:, 1), motionOutput.pos(:, 2), motionOutput.pos(:, 3), 'linewidth', lineWidth);
plot3(historyOutput.data(:, 1), historyOutput.data(:, 2), historyOutput.data(:, 3), '--', 'linewidth', lineWidth);

grid(ax, 'on')
legend('Input', 'Motion Model', 'Data History');
%view(ax, [-30 60])
title([sTrajectoryName ' - Positions'])
xlabel('x [m]')
ylabel('y [m]')
zlabel('x [m]')

%% Plot distance from origin vs time
f = figure;
ax = axes(f);
hold(ax, 'on');
plot(historyInput.time, vecnorm(historyInput.data, 2, 2), '-kx', 'linewidth', 1.25);
plot(motionOutput.time, vecnorm(motionOutput.pos, 2, 2), 'linewidth', lineWidth);
plot(historyOutput.time, vecnorm(historyOutput.data, 2, 2), '--', 'linewidth', lineWidth);

grid(ax, 'on')
legend('Input', 'Motion Model', 'Data History');

xlabel('Time [s]')
ylabel('Distance from origin [m]')
title([sTrajectoryName ' - Distance'])