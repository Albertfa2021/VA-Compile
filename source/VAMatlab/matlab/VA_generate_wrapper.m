function VA_generate_wrapper(va_script_dir, templateFile)
    % Parameters
    outputFile = fullfile( va_script_dir, 'VA.m' );

    fprintf( 'Generating code for VA Matlab class ''%s'' ...\n', outputFile );
    code = fileread( templateFile );
    stubCode = VA_generate_stubs();

    code = strrep( code, '###STUBCODE###', stubCode );

    % Write the results
    fid = fopen( outputFile, 'w' );
    fprintf( fid, '%s', code );
    fclose( fid );

    fprintf( 'Matlab class ''%s'' successfully built\n', outputFile );
