#
#  Info: Only input parameters via call by value possible!
#
$INTERFACE_DEFINITION = "VACore.interface.txt";
$CODE_TEMPLATE = "template.cpp";

@RETURN_TYPES = ("void",
                 "bool",
				 "int",
				 "double",
				 "std::string");
				 
@INPUT_TYPES =  ("void",
                 "bool",
				 "int",
				 "double",
				 "const std::string&");
				 
@FUNCTION_MODIFIERS = ("const");

# Read a file into a scalar
sub readFile {
	($filename) = @_;
	open (FILE, "<$filename") || die $!;
	$data = do { local $/; <FILE> };
	close FILE;
	return $data;
}
				 
# Parse interface definition
open (in, "<$INTERFACE_DEFINITION") || die $!;
$n=0;
@functions = ();
while (<in>){
	# in $_ steht die aktuelle Zeile
	$n++;
	chomp($_);
	#print "$n: $_\n";
	
	# Skip comments and empty lines
	next if ($_ =~ /(^\#.*)|^\w*$/);
	
	#print "Parsing line: $n: $_\n";	

	# Parse method definition
	# Format: RETURN_TYPE METHOD_NAME(MODIFIER PARAM_TYPE (REFSYMBOL)*) MODIFIER
	
	if ($_ =~ /^(.*)\s+(\w+?)\s*\((.*)\)\s*(.*)\s*\;\s*/) {
		$return_type = $1;
		$function_name = $2;
		@input_defs = split(/,/, $3);
		$function_modifier = $4;
		
		# Check the return type
		$return_type =~ s/^\s+//;
		$return_type =~ s/\s+$//;
		if (! grep /$return_type/, @RETURN_TYPES) {
			print STDERR "Invalid return type \"$return_type\" in line $n\n";
			exit 255;
		}
		
		# Parse the input parameters
		undef @input_params;
		foreach $input_def (@input_defs) {
			# Remove whitespaces
			$input_def =~ s/^\s+//;
			$input_def =~ s/\s+$//;

			#print "Input def: >$input_def<\n";
			
			# Check the input type
			if (! grep /$input_def/, @INPUT_TYPES) {
				print STDERR "Invalid input type \"$input_def\" in line $n\n";
				exit 255;
			}
			
			push(@input_params, $input_def);
			
			# Format: INPUT_TYPE (NAME)?
			#if ($input =~ /^\s*(\w+)(\s+\w+)?\s*$/) {
			# if ($input =~ /^(\.*)$/) {
				# print "TYPE = $1\n";
				# print "NAME = $2\n";
				# print "3 = $3\n";
			# } else {
				# print "NOmatch\n";
			# }
		}
		print "Input params: >".join(",", @input_params)."<\n";
		
		# Check the modifier (may only be const)
		$function_modifier =~ s/^\s+//;
		$function_modifier =~ s/\s+$//;
		if ($function_modifier && ! grep /$function_modifier/, @FUNCTION_MODIFIERS) {
			print STDERR "Invalid function modifier \"$function_modifier\" in line $n\n";
			exit 255;
		}
		print "Modifier: >$function_modifier<\n";

		# Store function description
		push(@functions, { return_type => $return_type,
		                   name => $function_name,
						   input_types => \@input_params,
						   modifier => $function_modifier } );
	}
}
close in;

# Print parsing results
print "Found ".scalar(@functions)." functions\n\n";
foreach $function (@functions) {
	# print "Function:\t".$function->{'name'}."\n";
	# print "Returns: \t".$function->{'return_type'}."\n";
	# print "Params:  \t".join(",", @{$function->{'input_types'}})."\n";
	# print "Modifier:\t".$function->{'modifier'}."\n";
	print $function->{'return_type'}." ".$function->{'name'}."(".join(",", @{$function->{'input_types'}}).") ".$function->{'modifier'}."\n";
	
}
# for $i (0..$#functions) {
	# @a = $functions[$i]{'input_types'};
	# print "a = @a\n";
	# print scalar(@a);
	# print $functions[$i]{'return_type'}." ".$functions[$i]{'name'}."(".join(",", $functions[$i]{'input_types'}).") ".$functions[$i]{'modifier'}."\n";
# }

#exit 0;

# -------------------------------------

sub generateMethodDeclaration {
	my %function = @_;
	
	# Format int NAME(lua_State* L);
	return sprintf("\tint %s(lua_State* L);\n", $function->{'name'});
}

sub generateMethodRegistration {
	my %function = @_;
	
	# Format method(CVALuaCoreObject, NAME),
	return sprintf("\tmethod(CVALuaCoreObject, %s),\n", $function->{'name'});
}

sub generateMethodImplementation {
	my %function = @_;
	
# int CVALuaCoreObject::LoadDirectivity(lua_State *L) {

	# /* [int] core:LoadDirectivity( filename, name ) */

	# VASHELL_TRY();

	# std::string sFilename;
	# std::string sName;

	# int argc = lua_gettop(L);
	# sFilename = luaL_checkstring(L, 1);
	# if (argc >= 2) sName = luaL_checkstring(L, 2);

	# lua_pushinteger(L, m_pCore->LoadDirectivity(sFilename, sName));
	# return 1;

	# VASHELL_CATCH();
# }	
	$rt = $function->{'return_type'};
	$n = $function->{'name'};
	$m = " ".$function->{'modifier'} if ($function->{'modifier'});
	$z = join(",", @{$function->{'input_types'}});
	
	# Signature
	$x .= sprintf("int CVALuaCoreObject::%s(lua_State* L) {\n", $n);
	
	# Comment
	$x .= sprintf("\t/* %s %s(%s)%s; */\n", $function->{'return_type'}, $n, $z, $m);
	
	$x .= sprintf("\n\tVASHELL_TRY();\n\n");
	
	# Input parameters
	$k=0;
	undef @y;
	foreach $t (@{$function->{'input_types'}}) {
		$k++;
		if ($t eq "bool") {
			$x .= sprintf("\t%s bParam%d = (luaL_checkint(L, %d) != 0);\n", $t, $k, $k);
			push(@y, sprintf("bParam%d", $k));
		}
		
		if ($t eq "int") {
			$x .= sprintf("\t%s iParam%d = luaL_checkint(L, %d);\n", $t, $k, $k);
			push(@y, sprintf("iParam%d", $k));
		}
		
		if ($t eq "double") {
			$x .= sprintf("\t%s dParam%d = luaL_checknumber(L, %d);\n", $t, $k, $k);
			push(@y, sprintf("dParam%d", $k));
		}

		if (($t eq "const std::string&") || ($t eq "std::string")) {
			$x .= sprintf("\tstd::string sParam%d = luaL_checkstring(L, %d);\n", $k, $k);
			push(@y, sprintf("sParam%d", $k));
		}	
	}
	
	# No return value
	if ($rt eq "void") {
		$x .= sprintf("\tm_pCore->%s(%s);\n", $n, join(", ", @y));
		$x .= sprintf("\treturn 0;\n");
	}
	
	# Boolean return value
	if ($rt eq "bool") {
		$x .= sprintf("\tlua_pushboolean(L, m_pCore->%s(%s) );\n", $n, join(", ", @y));
		$x .= sprintf("\treturn 1;\n");
	}
	
	# Integer return value
	if ($rt eq "int") {
		$x .= sprintf("\tlua_pushinteger(L, m_pCore->%s(%s) );\n", $n, join(", ", @y));
		$x .= sprintf("\treturn 1;\n");
	}
	
	# Double return value
	if ($rt eq "double") {
		$x .= sprintf("\tlua_pushnumber(L, m_pCore->%s(%s) );\n", $n, join(", ", @y));
		$x .= sprintf("\treturn 1;\n");
	}
	
	# String return value
	if ($rt eq "std::string") {
		$x .= sprintf("\tlua_pushnumber(L, m_pCore->%s(%s) );\n", $n, join(", ", @y));
		$x .= sprintf("\treturn 1;\n");
	}
	
	$x .= sprintf("\n\tVASHELL_CATCH();\n");
	$x .= sprintf("}\n\n");
	return $x;
}

# Build the code blocks
foreach $function (@functions) {
	$decl = $decl.generateMethodDeclaration($function);
	$regs = $regs.generateMethodRegistration($function);
	$impl = $impl.generateMethodImplementation($function);
}

# Load the template and substitute the macros
$out = readFile($CODE_TEMPLATE);
$out =~ s/\/\*\*\* GENERATE_METHOD_DECLARATIONS \*\*\*\//$decl/g;
$out =~ s/\/\*\*\* GENERATE_METHOD_REGISTRATIONS \*\*\*\//$regs/g;
$out =~ s/\/\*\*\* GENERATE_METHOD_IMPLEMENTATIONS \*\*\*\//$impl/g;
print "$out\n";
