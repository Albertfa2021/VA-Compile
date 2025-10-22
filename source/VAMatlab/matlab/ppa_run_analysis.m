%% Input conditions
target_path = 'path_003ca6d3a74540310873574908219251'; %path to follow through simulation
%target_path = 'diffracted_path';

input_folder = fullfile( 'D:\Users\stienen\Sciebo\Thesis stuff\auralization\resources', 'example_ppa/s1_r1' );
out_filename = 'Path_evolution.gif';
out_filename_freq = 'Path_evolution_freq.gif';
plot_single_path_freq = true;
plot_single_path = true;
plot_all_delay_changes = false;
plot_all_gain_changes = false;
load( fullfile( input_folder, 'analysis_data.mat'), '-mat' );

%% Initial set up
file_listing = dir( strcat( input_folder, '/frame*data.mat' ) );
N_frames = numel(file_listing) - 1;
N_paths = numel(all_path_names);

if plot_single_path_freq
    freq_evolution = zeros(N_frames,31);
    valid_evolution = zeros(N_frames,31);
end
if plot_single_path
    path_data = cell(1, N_frames);
end
if plot_all_delay_changes
    delay_changes = zeros(N_frames, N_paths);
    prev_delay = zeros(1,N_paths);
    path_altered = zeros(1,N_paths);
end
if plot_all_gain_changes
    gain_changes = zeros(N_frames, N_paths);
    prev_gain = zeros(1,N_paths);
end
f = ita_ANSI_center_frequencies;

%% Main loop
for n = 1:N_frames %loop over all frames
    file_name = sprintf( '/frame%04idata.mat', n );
    load(strcat(input_folder,file_name),'-mat'); %load path data for the next frame
    
    if plot_single_path_freq || plot_single_path
        try
            path = paths_update.(target_path);
            freq_evolution(n,:) = path.frequency_magnitudes;
            valid_evolution(n,:) = path.audible;
            
            path_data{n} = all_paths_data.(target_path);
            N_interaction_points = size(path_data{n},1);
        catch
            freq_evolution(n,:) = zeros(1,31);
            valid_evolution(n,:) = zeros(1,31);
        end
    end
    if plot_all_delay_changes || plot_all_gain_changes
        for p = 1:N_paths
            try
                path = paths_update.(all_path_names{p});
                if( path_altered(p) == 1 )
                    delay_changes(n,p) = path.delay - prev_delay(p);
                    prev_delay(p) = path.delay;

                    gain_changes(n,p) = path.gain - prev_gain(p);
                    prev_gain(p) = path.gain;
                else
                    path_altered(p) = 1;
                    prev_delay(p) = path.delay;
                    prev_gain(p) = path.gain;
                end
            catch
            end

        end
    end
end

%% Plot Graphs
if plot_single_path
    visualise_single_path( path_data, out_filename, N_interaction_points, f, freq_evolution, valid_evolution );
    %visualise_single_freq( f, N_frames, freq_evolution, valid_evolution, out_filename_freq )
end
if plot_single_path_freq
    single_path_freq_evolution( f, N_frames, freq_evolution, valid_evolution );
end
if plot_all_delay_changes
    all_path_delay_changes( delay_changes, N_paths, N_frames );
end
if plot_all_gain_changes
    all_path_gain_changes( gain_changes, N_paths, N_frames );
end


%% Plot functions
function single_path_freq_evolution( f, N, freq_evolution, valid_evolution )
    [X,Y] = meshgrid(f, 1:N);
    figure
    a = axes;
    s = surf( a, X, Y, freq_evolution, valid_evolution );
    set( a, 'ZScale', 'log' );
    set( s, 'EdgeAlpha', 0.05 );
    xlabel('Frequency (KHz)')
    ylabel('Frame number')
    zlabel('Frequency Magnitudes')
    grid off
end

function all_path_delay_changes( delay_changes, N_paths, N_frames )
    [X,Y] = meshgrid(1:N_paths, 1:N_frames);
    figure
    a = axes;
    s = surf( a, X, Y, delay_changes);
    set( s, 'EdgeAlpha', 0.05 );    
    ylabel('Frames')
    xlabel('Paths')
    zlabel('Delay Change')
    grid off
end

function all_path_gain_changes( gain_changes, N_paths, N_frames )
    [X,Y] = meshgrid(1:N_paths, 1:N_frames);
    figure
    a = axes;
    s = surf( a, X, Y, gain_changes);
    set( s, 'EdgeAlpha', 0.05 );
    ylabel('Frames')
    xlabel('Paths')
    zlabel('Gain Change')
    grid off
end

function visualise_single_path( path_data, out_filename, N_interaction_points, f, freq_evolution, valid_evolution )
%% Export giff of selected path

    fig1 = figure;
    fig1.Units = 'pixels';
    fig1.Position = [50 50 1000 400 ];
    a1 = subplot(1,2,1);
    axis equal;
    
    plot3([-5.1251,-5.1251],[-46.6484,53.5954],[20.8399,20.8399],'-k')
    hold on
    plot3([-12.6201,-12.6201],[-46.6484,53.5954],[28.2501,28.2501],'-k')
    plot3([-5.1251,-5.1251],[-46.6484,53.5954],[-0.1337,-0.1337],'-k')
    plot3([-5.1251,-5.1251],[-46.6484,-46.6484],[-0.1337,20.8399],'-k')
    plot3([-5.1251,-5.1251],[53.5954,53.5954],[-0.1337,20.8399],'-k')
    plot3([6.8749,6.8749],[-46.6484,-46.6484],[-0.1337,19.7963],'-k')
    plot3([6.8749,6.8749],[-46.6484,-5.6111],[19.7963,19.7963],'-k')
    plot3([6.8749,6.8749],[-5.6111,-5.6111],[19.7963,-0.1337],'-k')
    plot3([6.8749,80.3522],[-5.6111,-5.6111],[-0.1337,-0.1337],'-k')
    plot3([80.3522,80.3522],[-5.6111,-5.6111],[-0.1337,19.7963],'-k')
    plot3([80.3522,6.8749],[-5.6111,-5.6111],[19.7963,19.7963],'-k')
    plot3([6.8749,6.8749],[6.3889,6.3889],[19.7963,-0.1337],'-k')
    plot3([6.8749,6.8749],[6.3889,53.5954],[19.7963,19.7963],'-k')
    plot3([6.8749,80.3522],[6.3889,6.3889],[19.7963,19.7963],'-k')
    plot3([80.3522,80.3522],[6.3889,6.3889],[19.7963,-0.1337],'-k')
    plot3([80.3522,6.8749],[6.3889,6.3889],[-0.1337,-0.1337],'-k')
    plot3([6.8749,6.8749],[-46.6484,-5.6111],[-0.1337,-0.1337],'-k')
    plot3([6.8749,6.8749],[6.3889,53.5954],[-0.1337,-0.1337],'-k')
    plot3([6.8749,6.8749],[53.5954,53.5954],[-0.1337,19.7963],'-k')
    plot3([-5.1251,-12.6201],[53.5954,53.5954],[20.8399,28.2501],'-k')
    plot3([-5.1251,-12.6201],[-46.6484,-46.6484],[20.8399,28.2501],'-k')
    a1.XLim = [-10 20];
    a1.YLim = [-60 20];
    a1.ZLim = [-0.1337 20.8399];
    axis off;
    
    N_frames = numel(path_data);
    try
        source_point = plot3(path_data{1}(1,1),path_data{1}(1,2),path_data{1}(1,3),'.r');
        receiver_point = plot3(path_data{1}(N_interaction_points,1),path_data{1}(N_interaction_points,2),path_data{1}(N_interaction_points,3),'.g');
    catch
        source_point = plot3(-1000,-1000,-1000,'.r', 'MarkerSize', 20);
        receiver_point = plot3(-1000,-1000,-1000,'.g', 'MarkerSize', 20);
    end
    for i = 1:N_interaction_points-1
        try
            pointA = path_data{1}(i,:);
            pointB = path_data{1}(i+1,:);
            segment{i} = plot3(a1, [pointA(1),pointB(1)], [pointA(2),pointB(2)], [pointA(3),pointB(3)] );
        catch
            segment{i} =  plot3([-12.6201,-12.6201],[-46.6484,-46.6484],[28.2501,28.2501],'-b');
        end
    end
    
    a2 = subplot(1,2,2);
    if valid_evolution(1,1) == 1
        p = plot( a2, f, freq_evolution(1,:), 'k' );
    else
        p = plot( a2, f, freq_evolution(1,:), 'r' );
    end
    set( a2, 'YScale', 'log' );
    set( a2, 'XScale', 'log' );
    xlabel('Frequency (Hz)')
    ylabel('Frequency Magnitudes')
    grid off
    a2.YLim = [10^-6 1];
    a2.XLim = [min(f) max(f)];
    
    drawnow
    frame = getframe(fig1);
    im = frame2im(frame);
    [imind,cm] = rgb2ind(im,256);
    imwrite( imind, cm, out_filename, 'gif', 'Loopcount', inf, 'DelayTime', 0.05 );

    path_start = false;
    for n = 2:10:N_frames
        try
            source_point.XData = path_data{n}(1,1);
            source_point.YData = path_data{n}(1,2);
            source_point.ZData = path_data{n}(1,3);
            receiver_point.XData = path_data{n}(N_interaction_points,1);
            receiver_point.YData = path_data{n}(N_interaction_points,2);
            receiver_point.ZData = path_data{n}(N_interaction_points,3);
            for i = 1:N_interaction_points-1
                segment{i}.XData = [path_data{n}(i,1), path_data{n}(i+1,1)]; % re set the x/y/z data
                segment{i}.YData = [path_data{n}(i,2), path_data{n}(i+1,2)];
                segment{i}.ZData = [path_data{n}(i,3), path_data{n}(i+1,3)];
                path_start = true;     
            end
            
            p.YData = freq_evolution(n,:);
            if( valid_evolution(n,1) == 1 )
                p.Color = 'k';
            else
                p.Color = 'r';
            end
            
            drawnow
            frame = getframe(fig1);
            im = frame2im(frame);
            [imind,cm] = rgb2ind(im,256);
            imwrite( imind, cm, out_filename, 'gif', 'WriteMode', 'append', 'DelayTime', 0.05 );
        catch
            if path_start
                return
            end
        end
    end
    
end

function visualise_single_freq( f, N_frames, freq_evolution, valid_evolution, out_filename_freq )

%%
    fig = figure;
    a2 = axes;
    if valid_evolution(1,1) == 1
        p = plot( a2, f, freq_evolution(1,:), 'k' );
        start_plot = 1;
    else
        p = plot( a2, f, freq_evolution(1,:), 'r' );
        start_plot = 0;
    end
    set( a2, 'YScale', 'log' );
    xlabel('Frequency (KHz)')
    ylabel('Frequency Magnitudes')
    grid off
    a2.YLim = [10^-6 1];
    a2.XLim = [min(f) max(f)];
    
    drawnow
    frame = getframe(fig);
    im = frame2im(frame);
    [imind,cm] = rgb2ind(im,256);
    imwrite( imind, cm, out_filename_freq, 'gif', 'Loopcount', inf, 'DelayTime', 0.03 );
    
    no_path = 0;
    for n = 2:10:N_frames
        if( valid_evolution(n,1) == 1 )
            start_plot = 1;
            p.Color = 'k';
        else
            p.Color = 'r';
            if( start_plot == 1)
                no_path = no_path + 1;
            end
        end
        if( start_plot == 0 ) %skip time before target path kicks in
            continue
        end
        p.YData = freq_evolution(n,:);

        drawnow
        frame = getframe(fig);
        im = frame2im(frame);
        [imind,cm] = rgb2ind(im,256);
        imwrite( imind, cm, out_filename_freq, 'gif', 'WriteMode', 'append', 'DelayTime', 0.03 );
        if( (no_path>=10) && (start_plot==1) ) %cut off time after path has finished
            return
        end
    end
end