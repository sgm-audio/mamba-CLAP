with open("backend/worker.py", "r") as f:
    lines = f.readlines()

out = []
for i, line in enumerate(lines):
    # lines 94 to 240 (0-indexed 93 to 239)
    if 94 <= i + 1 <= 240:
        if line.strip() == "":
            out.append(line)
        else:
            out.append("    " + line)
    else:
        out.append(line)

# Now we need to insert '        try:\n' before line 95
out.insert(94, "        try:\n")

with open("backend/worker.py", "w") as f:
    f.writelines(out)
