#!/usr/bin/env python3
import json
import os
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULTS_FILE = os.path.join(REPO_ROOT, "testing", "test_results.json")
REPORT_FILE = os.path.join(REPO_ROOT, "testing", "report.html")

def generate_report():
    if not os.path.exists(RESULTS_FILE):
        print(f"Results file {RESULTS_FILE} not found. Run test_runner.py first.")
        return

    try:
        with open(RESULTS_FILE, "r") as f:
            results = json.load(f)
    except Exception as e:
        print(f"Error reading results: {e}")
        return

    if not results:
        print("No results found in the results file.")
        return

    passed = sum(1 for r in results if r["status"] == "PASS")
    failed = sum(1 for r in results if r["status"] == "FAIL")
    total = len(results)
    pass_pct = (passed / total * 100) if total > 0 else 0

    # Sort results: Failed first, then alphabetical
    results.sort(key=lambda x: (x["status"] == "PASS", x["package"]))

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>KDOS Build Integrity Report</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;700&family=JetBrains+Mono&display=swap" rel="stylesheet">
    <style>
        :root {{
            --bg-color: #0b0e14;
            --card-bg: rgba(22, 27, 34, 0.7);
            --text-color: #c9d1d9;
            --accent-color: #58a6ff;
            --pass-color: #3fb950;
            --fail-color: #f85149;
            --border-color: rgba(255, 255, 255, 0.1);
            --glass-bg: rgba(255, 255, 255, 0.03);
        }}

        body {{
            background-color: var(--bg-color);
            color: var(--text-color);
            font-family: 'Inter', sans-serif;
            margin: 0;
            line-height: 1.6;
            background-image: 
                radial-gradient(circle at 10% 20%, rgba(88, 166, 255, 0.05) 0%, transparent 40%),
                radial-gradient(circle at 90% 80%, rgba(248, 81, 73, 0.05) 0%, transparent 40%);
            background-attachment: fixed;
        }}

        header {{
            padding: 3rem 1rem;
            text-align: center;
            background: rgba(13, 17, 23, 0.8);
            backdrop-filter: blur(20px);
            border-bottom: 1px solid var(--border-color);
            position: sticky;
            top: 0;
            z-index: 100;
        }}

        h1 {{
            margin: 0;
            font-size: 2.5rem;
            font-weight: 800;
            letter-spacing: -1px;
            background: linear-gradient(90deg, #fff, #58a6ff);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }}

        .container {{
            max-width: 1200px;
            margin: 2rem auto;
            padding: 0 1.5rem;
        }}

        .summary-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
            gap: 1.5rem;
            margin-bottom: 3rem;
        }}

        .stat-card {{
            background: var(--card-bg);
            backdrop-filter: blur(15px);
            padding: 2rem;
            border-radius: 16px;
            border: 1px solid var(--border-color);
            box-shadow: 0 8px 32px rgba(0,0,0,0.3);
            text-align: center;
            transition: transform 0.3s ease;
        }}

        .stat-card:hover {{
            transform: translateY(-5px);
            border-color: var(--accent-color);
        }}

        .stat-val {{
            font-size: 3rem;
            font-weight: 800;
            display: block;
            margin-bottom: 0.5rem;
        }}

        .stat-label {{
            font-size: 0.85rem;
            color: #8b949e;
            text-transform: uppercase;
            letter-spacing: 2px;
            font-weight: 600;
        }}

        .search-container {{
            margin-bottom: 1.5rem;
            position: relative;
        }}

        #search {{
            width: 100%;
            padding: 1rem 1.5rem;
            background: var(--card-bg);
            border: 1px solid var(--border-color);
            border-radius: 12px;
            color: white;
            font-size: 1.1rem;
            outline: none;
            transition: border-color 0.3s;
            box-sizing: border-box;
        }}

        #search:focus {{
            border-color: var(--accent-color);
        }}

        .list-container {{
            background: var(--card-bg);
            backdrop-filter: blur(15px);
            border-radius: 16px;
            border: 1px solid var(--border-color);
            overflow: hidden;
            box-shadow: 0 8px 32px rgba(0,0,0,0.3);
        }}

        table {{
            width: 100%;
            border-collapse: collapse;
        }}

        th {{
            background: rgba(255, 255, 255, 0.05);
            text-align: left;
            padding: 1.25rem 1.5rem;
            font-size: 0.9rem;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: #8b949e;
        }}

        td {{
            padding: 1.25rem 1.5rem;
            border-top: 1px solid var(--border-color);
        }}

        .pkg-name {{
            font-weight: 600;
            font-size: 1.1rem;
            color: #fff;
        }}

        .badge {{
            padding: 0.4rem 0.8rem;
            border-radius: 20px;
            font-size: 0.75rem;
            font-weight: 800;
            text-transform: uppercase;
        }}

        .badge-pass {{ 
            background: rgba(63, 185, 80, 0.1); 
            color: var(--pass-color);
            border: 1px solid rgba(63, 185, 80, 0.2);
        }}

        .badge-fail {{ 
            background: rgba(248, 81, 73, 0.1); 
            color: var(--fail-color);
            border: 1px solid rgba(248, 81, 73, 0.2);
        }}

        .duration {{ 
            font-family: 'JetBrains Mono', monospace; 
            font-size: 0.9rem;
            color: #8b949e;
        }}

        .log-link {{
            color: var(--accent-color);
            text-decoration: none;
            display: inline-flex;
            align-items: center;
            gap: 0.5rem;
            font-weight: 600;
            font-size: 0.9rem;
        }}

        .log-link:hover {{
            text-decoration: underline;
        }}

        tr:hover {{
            background: rgba(255, 255, 255, 0.03);
        }}

        tr.fail-row {{
            background: rgba(248, 81, 73, 0.02);
        }}
    </style>
</head>
<body>
    <header>
        <h1>KDOS Build Integrity Report</h1>
        <p style="color: #8b949e; margin-top: 0.5rem;">Automated Isolation Testing Suite</p>
    </header>

    <div class="container">
        <div class="summary-grid">
            <div class="stat-card">
                <span class="stat-val">{total}</span>
                <span class="stat-label">Packages</span>
            </div>
            <div class="stat-card">
                <span class="stat-val" style="color: var(--pass-color)">{passed}</span>
                <span class="stat-label">Passed</span>
            </div>
            <div class="stat-card">
                <span class="stat-val" style="color: var(--fail-color)">{failed}</span>
                <span class="stat-label">Failed</span>
            </div>
            <div class="stat-card">
                <span class="stat-val" style="color: {var(--pass-color) if pass_pct > 90 else var(--fail-color)}">{pass_pct:.1f}%</span>
                <span class="stat-label">Integrity Score</span>
            </div>
        </div>

        <div class="search-container">
            <input type="text" id="search" placeholder="Filter by package name..." onkeyup="filterTable()">
        </div>

        <div class="list-container">
            <table id="pkgTable">
                <thead>
                    <tr>
                        <th>Package Name</th>
                        <th>Build Status</th>
                        <th>Duration</th>
                        <th>Artifacts</th>
                    </tr>
                </thead>
                <tbody>
"""
    for res in results:
        badge_class = "badge-pass" if res["status"] == "PASS" else "badge-fail"
        row_class = "fail-row" if res["status"] == "FAIL" else ""
        html += f"""
                    <tr class="{row_class}">
                        <td class="pkg-name">{res['package']}</td>
                        <td><span class="badge {badge_class}">{res['status']}</span></td>
                        <td class="duration">{res['duration']:.2f}s</td>
                        <td><a href="{res['log_path']}" class="log-link" target="_blank">View Logs</a></td>
                    </tr>"""

    html += """
                </tbody>
            </table>
        </div>
    </div>

    <script>
        function filterTable() {
            let input = document.getElementById("search");
            let filter = input.value.toUpperCase();
            let table = document.getElementById("pkgTable");
            let tr = table.getElementsByTagName("tr");

            for (let i = 1; i < tr.length; i++) {
                let td = tr[i].getElementsByTagName("td")[0];
                if (td) {
                    let txtValue = td.textContent || td.innerText;
                    if (txtValue.toUpperCase().indexOf(filter) > -1) {
                        tr[i].style.display = "";
                    } else {
                        tr[i].style.display = "none";
                    }
                }
            }
        }
    </script>
</body>
</html>"""

    with open(REPORT_FILE, "w") as f:
        f.write(html)
    print(f"Report generated: {REPORT_FILE}")

if __name__ == "__main__":
    generate_report()
